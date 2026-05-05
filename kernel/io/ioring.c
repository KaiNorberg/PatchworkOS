#include <kernel/cpu/syscall.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/path.h>
#include <kernel/io/ioring.h>
#include <kernel/io/irp.h>
#include <kernel/io/ops.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>

#include <libc/io.h>
#include <libc/list.h>
#include <time.h>

static inline bool ioring_acquire(ioring_ctx_t* ctx)
{
    ioring_ctx_flags_t expected = atomic_load(&ctx->flags);
    if (!(expected & IORING_CTX_BUSY) &&
        atomic_compare_exchange_strong(&ctx->flags, &expected, expected | IORING_CTX_BUSY))
    {
        return true;
    }

    return false;
}

static inline void ioring_release(ioring_ctx_t* ctx)
{
    atomic_fetch_and(&ctx->flags, ~IORING_CTX_BUSY);
}

static inline status_t ioring_map(ioring_ctx_t* ctx, process_t* process, ioring_id_t id, ioring_t* userRing,
    void* address, size_t sentries, size_t centries)
{
    ioring_t* kernelRing = &ctx->ring;

    size_t pageAmount =
        BYTES_TO_PAGES(sizeof(ioring_ctrl_t) + (sentries * sizeof(iosqe_t)) + (centries * sizeof(iocqe_t)));
    if (pageAmount >= CONFIG_MAX_RINGS_PAGES)
    {
        return ERR(IO, TOOBIG);
    }

    pfn_t pages[CONFIG_MAX_RINGS_PAGES];
    if (!pmm_alloc_pages(pages, pageAmount))
    {
        return ERR(IO, NOMEM);
    }

    for (size_t i = 0; i < pageAmount; i++)
    {
        memset(PFN_TO_VIRT(pages[i]), 0, PAGE_SIZE);
    }

    // PML_OWNED means that the pages will be freed when unmapped.
    void* kernelAddr = NULL;
    status_t status =
        vmm_map_pages(NULL, &kernelAddr, pages, pageAmount, PML_WRITE | PML_PRESENT | PML_OWNED, NULL, NULL);
    if (IS_ERR(status))
    {
        pmm_free_pages(pages, pageAmount);
        return status;
    }

    void* userAddr = address;
    status =
        vmm_map_pages(&process->space, &userAddr, pages, pageAmount, PML_WRITE | PML_PRESENT | PML_USER, NULL, NULL);
    if (IS_ERR(status))
    {
        vmm_unmap(NULL, kernelAddr, pageAmount * PAGE_SIZE);
        return status;
    }

    ioring_ctrl_t* ctrl = (ioring_ctrl_t*)kernelAddr;
    atomic_init(&ctrl->shead, 0);
    atomic_init(&ctrl->stail, 0);
    atomic_init(&ctrl->ctail, 0);
    atomic_init(&ctrl->chead, 0);
    for (size_t i = 0; i < IOSQE_REGS_MAX; i++)
    {
        atomic_init(&ctrl->regs[i], 0);
    }

    userRing->ctrl = userAddr;
    userRing->id = id;
    userRing->squeue = (iosqe_t*)((uintptr_t)userAddr + sizeof(ioring_ctrl_t));
    userRing->sentries = sentries;
    userRing->smask = sentries - 1;
    userRing->cqueue = (iocqe_t*)((uintptr_t)userAddr + sizeof(ioring_ctrl_t) + (sentries * sizeof(iosqe_t)));
    userRing->centries = centries;
    userRing->cmask = centries - 1;

    kernelRing->ctrl = kernelAddr;
    kernelRing->id = id;
    kernelRing->squeue = (iosqe_t*)((uintptr_t)kernelAddr + sizeof(ioring_ctrl_t));
    kernelRing->sentries = sentries;
    kernelRing->smask = sentries - 1;
    kernelRing->cqueue = (iocqe_t*)((uintptr_t)kernelAddr + sizeof(ioring_ctrl_t) + (sentries * sizeof(iosqe_t)));
    kernelRing->centries = centries;
    kernelRing->cmask = centries - 1;

    ctx->process = process; // No reference
    ctx->userAddr = userAddr;
    ctx->kernelAddr = kernelAddr;
    ctx->pageAmount = pageAmount;

    atomic_fetch_or(&ctx->flags, IORING_CTX_MAPPED);
    return OK;
}

static inline void ioring_unmap(ioring_ctx_t* ctx)
{
    vmm_unmap(&ctx->process->space, ctx->userAddr, ctx->pageAmount * PAGE_SIZE);
    vmm_unmap(NULL, ctx->kernelAddr, ctx->pageAmount * PAGE_SIZE);

    atomic_fetch_and(&ctx->flags, ~IORING_CTX_MAPPED);
}

static inline uint64_t ioring_avail_cqe(ioring_ctx_t* ctx)
{
    ioring_t* ring = &ctx->ring;
    uint32_t ctail = atomic_load_explicit(&ring->ctrl->ctail, memory_order_relaxed);
    uint32_t chead = atomic_load_explicit(&ring->ctrl->chead, memory_order_acquire);
    return ctail - chead;
}

void ioring_ctx_init(ioring_ctx_t* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->ring = (ioring_t){0};
    ctx->process = NULL;
    list_init(&ctx->active);
    lock_init(&ctx->lock);
    atomic_init(&ctx->activeCount, 0);
    ctx->userAddr = NULL;
    ctx->kernelAddr = NULL;
    ctx->pageAmount = 0;
    wait_queue_init(&ctx->waitQueue);
    atomic_init(&ctx->flags, IORING_CTX_NONE);
}

void ioring_ctx_deinit(ioring_ctx_t* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (!ioring_acquire(ctx))
    {
        panic(NULL, "failed to acquire async context for deinitialization");
    }

    if (atomic_load(&ctx->flags) & IORING_CTX_MAPPED)
    {
        ioring_unmap(ctx);
    }

    ioring_release(ctx);
    wait_queue_deinit(&ctx->waitQueue);
}

static void ioring_commit_cqe(ioring_ctx_t* ctx, iosqe_t* sqe, status_t status, uintptr_t result)
{
    ioring_t* ring = &ctx->ring;

    iosqe_flags_t reg = (sqe->flags >> IOSQE_SAVE) & IOSQE_REG_MASK;
    if (reg != IOSQE_REG_NONE)
    {
        atomic_store_explicit(&ring->ctrl->regs[reg - 1], result, memory_order_release);
    }

    uint32_t tail = atomic_load_explicit(&ring->ctrl->ctail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&ring->ctrl->chead, memory_order_acquire);

    if ((tail - head) >= ring->centries)
    {
        LOG_ERR("async completion queue overflow pid=%llu\n", process_current()->id);
        return;
    }

    if (sqe->op == 6 && result == 22 && status == 150995214)
    {
        LOG_DEBUG("ioring_commit_cqe: op=%d, data=%p, status=%d, result=%llu\n", sqe->op, sqe->data, status, result);
    }

    iocqe_t* cqe = &ring->cqueue[tail & ring->cmask];
    cqe->op = sqe->op;
    cqe->data = sqe->data;
    cqe->status = status;
    cqe->result = result;

    atomic_store_explicit(&ring->ctrl->ctail, tail + 1, memory_order_release);
    wait_unblock(&ctx->waitQueue, WAIT_ALL, OK);
}

static status_t ioring_sqe_dummy(irp_t* irp)
{
    UNUSED(irp);
    return ERR(IO, CANCELLED);
}

static status_t ioring_complete(irp_t* irp, void* _ptr)
{
    UNUSED(_ptr);

    ioring_ctx_t* ctx = irp->ctx;
    ioring_commit_cqe(ctx, &irp->sqe, irp->status, irp->result);
    
    if (IS_ERR(irp->status) && !(irp->sqe.flags & IOSQE_HARDLINK))
    {        
        irp_t* next = irp_chain_next(irp);
        if (next != NULL)
        {
            irp_set_complete(next, ioring_complete, NULL);
            irp_call(next, ioring_sqe_dummy);
        }
    }
    else
    {
        irp_t* next = irp_chain_next(irp);
        if (next != NULL)
        {
            irp_set_complete(next, ioring_complete, NULL);
            irp_call(next, io_op_dispatch);
        }
    }

    lock_acquire(&ctx->lock);
    list_remove(&irp->activeEntry);
    lock_release(&ctx->lock);

    atomic_fetch_sub(&ctx->activeCount, 1);

    return OK;
}

typedef struct
{
    list_t irps;
    irp_t* link;
} ioring_notify_ctx_t;

static status_t ioring_sqe_pop(ioring_ctx_t* ctx, ioring_notify_ctx_t* notify)
{
    ioring_t* ring = &ctx->ring;

    uint32_t stail = atomic_load_explicit(&ring->ctrl->stail, memory_order_acquire);
    uint32_t shead = atomic_load_explicit(&ring->ctrl->shead, memory_order_relaxed);

    if (shead == stail)
    {
        return ERR(IO, AGAIN);
    }

    if (atomic_load(&ctx->activeCount) >= ring->centries)
    {
        return ERR(IO, NOSPACE);
    }
    atomic_fetch_add(&ctx->activeCount, 1);

    irp_t* irp = irp_new(ctx->process, ctx);
    if (irp == NULL)
    {
        atomic_fetch_sub(&ctx->activeCount, 1);
        return ERR(IO, NOMEM);
    }

    irp->sqe = ring->squeue[shead & ring->smask];
    irp->timeout = irp->sqe.timeout;

    atomic_store_explicit(&ring->ctrl->shead, shead + 1, memory_order_release);

    if (notify->link != NULL)
    {
        notify->link->next = irp;
        notify->link = NULL;
    }
    else
    {
        list_push_back(&notify->irps, &irp->entry);
    }

    if (irp->sqe.flags & IOSQE_LINK || irp->sqe.flags & IOSQE_HARDLINK)
    {
        notify->link = irp;
    }

    lock_acquire(&ctx->lock);
    list_push_back(&ctx->active, &irp->activeEntry);
    lock_release(&ctx->lock);

    return OK;
}

static status_t ioring_notify(ioring_ctx_t* ctx, size_t amount, size_t wait, size_t* processed)
{
    if (amount == 0 && wait == 0)
    {
        return OK;
    }

    if (!ioring_acquire(ctx))
    {
        return ERR(IO, ACQUIRED);
    }

    if (!(atomic_load(&ctx->flags) & IORING_CTX_MAPPED))
    {
        ioring_release(ctx);
        return ERR(IO, NOT_INIT);
    }

    ioring_notify_ctx_t notify = {
        .irps = LIST_CREATE(notify.irps),
        .link = NULL,
    };

    status_t status = OK;
    size_t count = 0;
    while (count < amount)
    {
        status = ioring_sqe_pop(ctx, &notify);
        if (IS_ERR(status))
        {
            break;
        }
        count++;
    }

    while (!list_is_empty(&notify.irps))
    {
        irp_t* irp = CONTAINER_OF(list_pop_front(&notify.irps), irp_t, entry);
        irp_set_complete(irp, ioring_complete, NULL);
        irp_call(irp, io_op_dispatch);
    }

    if (processed != NULL)
    {
        *processed = count;
    }

    if (wait == 0)
    {
        ioring_release(ctx);
        return status;
    }

    status_t waitStatus = WAIT_BLOCK(&ctx->waitQueue, ioring_avail_cqe(ctx) >= wait);
    if (IS_ERR(waitStatus))
    {
        ioring_release(ctx);
        return waitStatus;
    }

    ioring_release(ctx);
    return status;
}

SYSCALL_DEFINE(SYS_IORING_SETUP, ioring_t* userRing, void* address, size_t sentries, size_t centries)
{
    if (userRing == NULL || sentries == 0 || centries == 0 || !IS_POW2(sentries) || !IS_POW2(centries))
    {
        return ERR(IO, INVAL);
    }

    process_t* process = process_current();

    ioring_ctx_t* ctx = NULL;
    ioring_id_t id = 0;
    for (id = 0; id < ARRAY_SIZE(process->rings); id++)
    {
        ioring_ctx_flags_t expected = IORING_CTX_NONE;
        if (atomic_compare_exchange_strong(&process->rings[id].flags, &expected, IORING_CTX_BUSY))
        {
            ctx = &process->rings[id];
            break;
        }
    }

    if (ctx == NULL)
    {
        return ERR(IO, NOSPACE);
    }

    status_t status = ioring_map(ctx, process, id, userRing, address, sentries, centries);
    ioring_release(ctx);
    return status;
}

SYSCALL_DEFINE(SYS_IORING_TEARDOWN, ioring_id_t id)
{
    process_t* process = process_current();
    if (id >= ARRAY_SIZE(process->rings))
    {
        return ERR(IO, INVAL);
    }

    ioring_ctx_t* ctx = &process->rings[id];
    if (!ioring_acquire(ctx))
    {
        return ERR(IO, ACQUIRED);
    }

    if (!(atomic_load(&ctx->flags) & IORING_CTX_MAPPED))
    {
        ioring_release(ctx);
        return ERR(IO, NOT_INIT);
    }

    while (true)
    {
        bool found = false;
        lock_acquire(&ctx->lock);
        irp_t* target;
        LIST_FOR_EACH(target, &ctx->active, activeEntry)
        {
            irp_cancel_t handler = irp_cancel_claim(target);
            if (handler != NULL)
            {
                lock_release(&ctx->lock);
                irp_cancel_finish(target, handler);
                found = true;
                break;
            }
        }

        if (!found)
        {
            lock_release(&ctx->lock);
            break;
        }
    }

    while (atomic_load(&ctx->activeCount) != 0)
    {
        ASM("pause");
    }

    ioring_unmap(ctx);
    ioring_release(ctx);
    return OK;
}

SYSCALL_DEFINE(SYS_IORING_ENTER, ioring_id_t id, size_t amount, size_t wait)
{
    process_t* process = process_current();
    if (id >= ARRAY_SIZE(process->rings))
    {
        return ERR(IO, INVAL);
    }

    ioring_ctx_t* ctx = &process->rings[id];
    return ioring_notify(ctx, amount, wait, _result);
}