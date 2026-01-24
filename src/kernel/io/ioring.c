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

#include <errno.h>
#include <sys/ioring.h>
#include <sys/list.h>
#include <time.h>

static inline uint64_t ioring_ctx_acquire(ioring_ctx_t* ctx)
{
    ioring_ctx_flags_t expected = atomic_load(&ctx->flags);
    if (!(expected & IORING_CTX_BUSY) &&
        atomic_compare_exchange_strong(&ctx->flags, &expected, expected | IORING_CTX_BUSY))
    {
        return 0;
    }

    return ERR;
}

static inline void ioring_ctx_release(ioring_ctx_t* ctx)
{
    atomic_fetch_and(&ctx->flags, ~IORING_CTX_BUSY);
}

static inline uint64_t ioring_ctx_map(ioring_ctx_t* ctx, process_t* process, ioring_id_t id, ioring_t* userRing,
    void* address, size_t sentries, size_t centries)
{
    ioring_t* kernelRing = &ctx->ring;

    size_t pageAmount = BYTES_TO_PAGES(sizeof(ioring_ctrl_t) + (sentries * sizeof(sqe_t)) + (centries * sizeof(cqe_t)));
    if (pageAmount >= CONFIG_MAX_RINGS_PAGES)
    {
        errno = ENOMEM;
        return ERR;
    }

    if (centries >= POOL_IDX_MAX)
    {
        errno = EINVAL;
        return ERR;
    }

    pfn_t pages[CONFIG_MAX_RINGS_PAGES];
    if (pmm_alloc_pages(pages, pageAmount) == ERR)
    {
        errno = ENOMEM;
        return ERR;
    }

    for (size_t i = 0; i < pageAmount; i++)
    {
        memset(PFN_TO_VIRT(pages[i]), 0, PAGE_SIZE);
    }

    // PML_OWNED means that the pages will be freed when unmapped.
    void* kernelAddr = vmm_map_pages(NULL, NULL, pages, pageAmount, PML_WRITE | PML_PRESENT | PML_OWNED, NULL, NULL);
    if (kernelAddr == NULL)
    {
        pmm_free_pages(pages, pageAmount);
        return ERR;
    }

    void* userAddr =
        vmm_map_pages(&process->space, address, pages, pageAmount, PML_WRITE | PML_PRESENT | PML_USER, NULL, NULL);
    if (userAddr == NULL)
    {
        vmm_unmap(NULL, kernelAddr, pageAmount * PAGE_SIZE);
        return ERR;
    }

    irp_pool_t* irps = irp_pool_new(centries, process, ctx);
    if (irps == NULL)
    {
        vmm_unmap(&process->space, userAddr, pageAmount * PAGE_SIZE);
        vmm_unmap(NULL, kernelAddr, pageAmount * PAGE_SIZE);
        return ERR;
    }

    ioring_ctrl_t* ctrl = (ioring_ctrl_t*)kernelAddr;
    atomic_init(&ctrl->shead, 0);
    atomic_init(&ctrl->stail, 0);
    atomic_init(&ctrl->ctail, 0);
    atomic_init(&ctrl->chead, 0);
    for (size_t i = 0; i < SQE_REGS_MAX; i++)
    {
        atomic_init(&ctrl->regs[i], 0);
    }

    userRing->ctrl = userAddr;
    userRing->id = id;
    userRing->squeue = (sqe_t*)((uintptr_t)userAddr + sizeof(ioring_ctrl_t));
    userRing->sentries = sentries;
    userRing->smask = sentries - 1;
    userRing->cqueue = (cqe_t*)((uintptr_t)userAddr + sizeof(ioring_ctrl_t) + (sentries * sizeof(sqe_t)));
    userRing->centries = centries;
    userRing->cmask = centries - 1;

    kernelRing->ctrl = kernelAddr;
    kernelRing->id = id;
    kernelRing->squeue = (sqe_t*)((uintptr_t)kernelAddr + sizeof(ioring_ctrl_t));
    kernelRing->sentries = sentries;
    kernelRing->smask = sentries - 1;
    kernelRing->cqueue = (cqe_t*)((uintptr_t)kernelAddr + sizeof(ioring_ctrl_t) + (sentries * sizeof(sqe_t)));
    kernelRing->centries = centries;
    kernelRing->cmask = centries - 1;

    ctx->irps = irps;
    ctx->userAddr = userAddr;
    ctx->kernelAddr = kernelAddr;
    ctx->pageAmount = pageAmount;

    atomic_fetch_or(&ctx->flags, IORING_CTX_MAPPED);
    return 0;
}

static inline uint64_t ioring_ctx_unmap(ioring_ctx_t* ctx)
{
    vmm_unmap(&ctx->irps->process->space, ctx->userAddr, ctx->pageAmount * PAGE_SIZE);
    vmm_unmap(NULL, ctx->kernelAddr, ctx->pageAmount * PAGE_SIZE);

    irp_pool_free(ctx->irps);
    ctx->irps = NULL;

    atomic_fetch_and(&ctx->flags, ~IORING_CTX_MAPPED);
    return 0;
}

static inline uint64_t ioring_ctx_avail_cqes(ioring_ctx_t* ctx)
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
    ctx->irps = NULL;
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

    if (ioring_ctx_acquire(ctx) == ERR)
    {
        panic(NULL, "failed to acquire async context for deinitialization");
    }

    if (atomic_load(&ctx->flags) & IORING_CTX_MAPPED)
    {
        if (ioring_ctx_unmap(ctx) == ERR)
        {
            panic(NULL, "failed to deinitialize async context");
        }
    }

    ioring_ctx_release(ctx);
    wait_queue_deinit(&ctx->waitQueue);
}

static void ioring_ctx_complete(irp_t* irp, void* _ptr)
{
    UNUSED(_ptr);

    ioring_ctx_t* ctx = irp_get_ctx(irp);
    ioring_t* ring = &ctx->ring;

    sqe_flags_t reg = (irp->sqe.flags >> SQE_SAVE) & SQE_REG_MASK;
    if (reg != SQE_REG_NONE)
    {
        atomic_store_explicit(&ring->ctrl->regs[reg - 1], irp->res._raw, memory_order_release);
    }

    uint32_t tail = atomic_load_explicit(&ring->ctrl->ctail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&ring->ctrl->chead, memory_order_acquire);

    if ((tail - head) >= ring->centries)
    {
        /// @todo Handle overflow properly.
        panic(NULL, "Async completion queue overflow");
    }

    cqe_t* cqe = &ring->cqueue[tail & ring->cmask];
    cqe->op = irp->sqe.op;
    cqe->error = irp->err;
    cqe->data = irp->sqe.data;
    cqe->_result = irp->res._raw;

    atomic_store_explicit(&ring->ctrl->ctail, tail + 1, memory_order_release);
    wait_unblock(&ctx->waitQueue, WAIT_ALL, EOK);

    if (irp->err != EOK && !(irp->sqe.flags & SQE_HARDLINK))
    {
        while (true)
        {
            irp_t* next = irp_chain_next(irp);
            if (next == NULL)
            {
                break;
            }

            irp_complete(next);
        }
    }
    else
    {
        irp_t* next = irp_chain_next(irp);
        if (next != NULL)
        {
            irp_set_complete(next, ioring_ctx_complete, NULL);
            irp_call_direct(next, io_op_dispatch);
        }
    }

    irp_complete(irp);
}

static errno_t nop_cancel(irp_t* irp)
{
    irp_complete(irp);
    return EOK;
}

typedef struct
{
    list_t irps;
    irp_t* link;
} ioring_ctx_notify_ctx_t;

static uint64_t ioring_ctx_sqe_pop(ioring_ctx_t* ctx, ioring_ctx_notify_ctx_t* notify)
{
    ioring_t* ring = &ctx->ring;

    uint32_t stail = atomic_load_explicit(&ring->ctrl->stail, memory_order_acquire);
    uint32_t shead = atomic_load_explicit(&ring->ctrl->shead, memory_order_relaxed);

    if (shead == stail)
    {
        errno = EAGAIN;
        return ERR;
    }

    irp_t* irp = irp_get(ctx->irps);
    if (irp == NULL)
    {
        return ERR;
    }
    irp->sqe = ring->squeue[shead & ring->smask];

    atomic_store_explicit(&ring->ctrl->shead, shead + 1, memory_order_release);

    if (notify->link != NULL)
    {
        notify->link->next = irp->index;
        notify->link = NULL;
    }
    else
    {
        list_push_back(&notify->irps, &irp->entry);
    }

    if (irp->sqe.flags & SQE_LINK || irp->sqe.flags & SQE_HARDLINK)
    {
        notify->link = irp;
    }

    return 0;
}

uint64_t ioring_ctx_notify(ioring_ctx_t* ctx, size_t amount, size_t wait)
{
    if (amount == 0)
    {
        return 0;
    }

    if (ioring_ctx_acquire(ctx) == ERR)
    {
        errno = EBUSY;
        return ERR;
    }

    if (!(atomic_load(&ctx->flags) & IORING_CTX_MAPPED))
    {
        ioring_ctx_release(ctx);
        errno = EINVAL;
        return ERR;
    }

    ioring_ctx_notify_ctx_t notify = {
        .irps = LIST_CREATE(notify.irps),
        .link = NULL,
    };

    size_t processed = 0;
    while (processed < amount)
    {
        if (ioring_ctx_sqe_pop(ctx, &notify) == ERR)
        {
            break;
        }
        processed++;
    }

    while (!list_is_empty(&notify.irps))
    {
        irp_t* irp = CONTAINER_OF(list_pop_front(&notify.irps), irp_t, entry);

        irp_set_complete(irp, ioring_ctx_complete, NULL);
        irp_call_direct(irp, io_op_dispatch);
    }

    if (wait == 0)
    {
        ioring_ctx_release(ctx);
        return processed;
    }

    if (WAIT_BLOCK(&ctx->waitQueue, ioring_ctx_avail_cqes(ctx) >= wait) == ERR)
    {
        ioring_ctx_release(ctx);
        return processed > 0 ? processed : ERR;
    }

    ioring_ctx_release(ctx);
    return processed;
}

SYSCALL_DEFINE(SYS_IORING_SETUP, ioring_id_t, ioring_t* userRing, void* address, size_t sentries, size_t centries)
{
    if (userRing == NULL || sentries == 0 || centries == 0 || !IS_POW2(sentries) || !IS_POW2(centries))
    {
        errno = EINVAL;
        return ERR;
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
        errno = EMFILE;
        return ERR;
    }

    if (ioring_ctx_map(ctx, process, id, userRing, address, sentries, centries) == ERR)
    {
        ioring_ctx_release(ctx);
        return ERR;
    }

    ioring_ctx_release(ctx);
    return id;
}

SYSCALL_DEFINE(SYS_IORING_TEARDOWN, uint64_t, ioring_id_t id)
{
    process_t* process = process_current();
    if (id >= ARRAY_SIZE(process->rings))
    {
        errno = EINVAL;
        return ERR;
    }

    ioring_ctx_t* ctx = &process->rings[id];
    if (ioring_ctx_acquire(ctx) == ERR)
    {
        errno = EBUSY;
        return ERR;
    }

    if (!(atomic_load(&ctx->flags) & IORING_CTX_MAPPED))
    {
        ioring_ctx_release(ctx);
        errno = EINVAL;
        return ERR;
    }

    if (ctx->irps != NULL && atomic_load(&ctx->irps->pool.used) != 0)
    {
        irp_pool_cancel_all(ctx->irps);

        // Operations that can be cancelled should have been cancelled immediately.
        if (atomic_load(&ctx->irps->pool.used) != 0)
        {
            ioring_ctx_release(ctx);
            errno = EBUSY;
            return ERR;
        }
    }

    if (ioring_ctx_unmap(ctx) == ERR)
    {
        ioring_ctx_release(ctx);
        return ERR;
    }

    ioring_ctx_release(ctx);
    return 0;
}

SYSCALL_DEFINE(SYS_IORING_ENTER, uint64_t, ioring_id_t id, size_t amount, size_t wait)
{
    process_t* process = process_current();
    if (id >= ARRAY_SIZE(process->rings))
    {
        errno = EINVAL;
        return ERR;
    }

    ioring_ctx_t* ctx = &process->rings[id];
    return ioring_ctx_notify(ctx, amount, wait);
}