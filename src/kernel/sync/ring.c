#include <kernel/cpu/syscall.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/path.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/mem_desc.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sync/irp.h>
#include <kernel/sync/ring.h>

#include <errno.h>
#include <sys/list.h>
#include <sys/uring.h>
#include <time.h>

static inline uint64_t ring_ctx_acquire(ring_ctx_t* ctx)
{
    ring_ctx_flags_t expected = atomic_load(&ctx->flags);
    if (!(expected & RING_CTX_BUSY) && atomic_compare_exchange_strong(&ctx->flags, &expected, expected | RING_CTX_BUSY))
    {
        return 0;
    }

    return ERR;
}

static inline void ring_ctx_release(ring_ctx_t* ctx)
{
    atomic_fetch_and(&ctx->flags, ~RING_CTX_BUSY);
}

static inline uint64_t ring_ctx_map(ring_ctx_t* ctx, space_t* space, ring_id_t id, ring_t* userRing, void* address,
    size_t sentries, size_t centries)
{
    ring_t* kernelRing = &ctx->ring;

    size_t pageAmount = BYTES_TO_PAGES(sizeof(ring_ctrl_t) + (sentries * sizeof(sqe_t)) + (centries * sizeof(cqe_t)));
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

    void* userAddr = vmm_map_pages(space, address, pages, pageAmount, PML_WRITE | PML_PRESENT | PML_USER, NULL, NULL);
    if (userAddr == NULL)
    {
        vmm_unmap(NULL, kernelAddr, pageAmount * PAGE_SIZE);
        return ERR;
    }

    irp_pool_t* irps = irp_pool_new(centries, ctx);
    if (irps == NULL)
    {
        vmm_unmap(space, userAddr, pageAmount * PAGE_SIZE);
        vmm_unmap(NULL, kernelAddr, pageAmount * PAGE_SIZE);
        return ERR;
    }

    mem_desc_pool_t* descs = mem_desc_pool_new(centries);
    if (descs == NULL)
    {
        irp_pool_free(irps);
        mem_desc_pool_free(descs);
        vmm_unmap(space, userAddr, pageAmount * PAGE_SIZE);
        vmm_unmap(NULL, kernelAddr, pageAmount * PAGE_SIZE);
        return ERR;
    }

    ring_ctrl_t* ctrl = (ring_ctrl_t*)kernelAddr;
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
    userRing->squeue = (sqe_t*)((uintptr_t)userAddr + sizeof(ring_ctrl_t));
    userRing->sentries = sentries;
    userRing->smask = sentries - 1;
    userRing->cqueue = (cqe_t*)((uintptr_t)userAddr + sizeof(ring_ctrl_t) + (sentries * sizeof(sqe_t)));
    userRing->centries = centries;
    userRing->cmask = centries - 1;

    kernelRing->ctrl = kernelAddr;
    kernelRing->id = id;
    kernelRing->squeue = (sqe_t*)((uintptr_t)kernelAddr + sizeof(ring_ctrl_t));
    kernelRing->sentries = sentries;
    kernelRing->smask = sentries - 1;
    kernelRing->cqueue = (cqe_t*)((uintptr_t)kernelAddr + sizeof(ring_ctrl_t) + (sentries * sizeof(sqe_t)));
    kernelRing->centries = centries;
    kernelRing->cmask = centries - 1;

    ctx->irps = irps;
    ctx->descs = descs;
    ctx->userAddr = userAddr;
    ctx->kernelAddr = kernelAddr;
    ctx->pageAmount = pageAmount;
    ctx->space = space;

    atomic_fetch_or(&ctx->flags, RING_CTX_MAPPED);
    return 0;
}

static inline uint64_t ring_ctx_unmap(ring_ctx_t* ctx)
{
    irp_pool_free(ctx->irps);
    ctx->irps = NULL;

    mem_desc_pool_free(ctx->descs);
    ctx->descs = NULL;

    vmm_unmap(ctx->space, ctx->userAddr, ctx->pageAmount * PAGE_SIZE);
    vmm_unmap(NULL, ctx->kernelAddr, ctx->pageAmount * PAGE_SIZE);

    atomic_fetch_and(&ctx->flags, ~RING_CTX_MAPPED);
    return 0;
}

static inline uint64_t ring_ctx_avail_cqes(ring_ctx_t* ctx)
{
    ring_t* ring = &ctx->ring;
    uint32_t ctail = atomic_load_explicit(&ring->ctrl->ctail, memory_order_relaxed);
    uint32_t chead = atomic_load_explicit(&ring->ctrl->chead, memory_order_acquire);
    return ctail - chead;
}

void ring_ctx_init(ring_ctx_t* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->ring = (ring_t){0};
    ctx->irps = NULL;
    ctx->userAddr = NULL;
    ctx->kernelAddr = NULL;
    ctx->pageAmount = 0;
    ctx->space = NULL;
    wait_queue_init(&ctx->waitQueue);
    atomic_init(&ctx->flags, RING_CTX_NONE);
}

void ring_ctx_deinit(ring_ctx_t* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ring_ctx_acquire(ctx) == ERR)
    {
        panic(NULL, "failed to acquire async context for deinitialization");
    }

    if (atomic_load(&ctx->flags) & RING_CTX_MAPPED)
    {
        if (ring_ctx_unmap(ctx) == ERR)
        {
            panic(NULL, "failed to deinitialize async context");
        }
    }

    ring_ctx_release(ctx);
    wait_queue_deinit(&ctx->waitQueue);
}

static void ring_ctx_dispatch(irp_t* irp);

static void ring_ctx_complete(irp_t* irp, void* _ptr)
{
    UNUSED(_ptr);

    ring_ctx_t* ctx = irp_get_ctx(irp);
    ring_t* ring = &ctx->ring;

    sqe_flags_t reg = (irp->flags >> SQE_SAVE) & SQE_REG_MASK;
    if (reg != SQE_REG_NONE)
    {
        atomic_store_explicit(&ring->ctrl->regs[reg], irp->result, memory_order_release);
    }

    uint32_t tail = atomic_load_explicit(&ring->ctrl->ctail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&ring->ctrl->chead, memory_order_acquire);

    if ((tail - head) >= ring->centries)
    {
        /// @todo Handle overflow properly.
        panic(NULL, "Async completion queue overflow");
    }

    cqe_t* cqe = &ring->cqueue[tail & ring->cmask];
    cqe->verb = irp->verb;
    cqe->error = irp->err;
    cqe->data = irp->data;
    cqe->_result = irp->result;

    atomic_store_explicit(&ring->ctrl->ctail, tail + 1, memory_order_release);
    wait_unblock(&ctx->waitQueue, WAIT_ALL, EOK);

    if (irp->err != EOK && !(irp->flags & SQE_HARDLINK))
    {
        while (true)
        {
            irp_t* next = irp_next(irp);
            if (next == NULL)
            {
                break;
            }

            irp_free(next);
        }
    }
    else
    {
        irp_t* next = irp_next(irp);
        if (next != NULL)
        {
            ring_ctx_dispatch(next);
        }
    }

    irp_free(irp);

    if (atomic_load(&ctx->irps->pool.used) == 0)
    {
        UNREF(ctx->process);
        ctx->process = NULL;
    }
}

static void ring_ctx_dispatch(irp_t* irp)
{
    ring_ctx_t* ctx = irp_get_ctx(irp);
    ring_t* ring = &ctx->ring;

    for (uint64_t i = 0; i < SQE_MAX_ARGS; i++)
    {
        sqe_flags_t reg = (irp->flags >> (i * SQE_REG_SHIFT)) & SQE_REG_MASK;
        if (reg == SQE_REG_NONE)
        {
            continue;
        }

        irp->sqe._args[i] = atomic_load_explicit(&ring->ctrl->regs[reg], memory_order_acquire);
    }

    irp_push(irp, ring_ctx_complete, NULL);
    irp_dispatch(irp);
}

typedef struct
{
    list_t irps;
    irp_t* link;
} ring_ctx_notify_ctx_t;

static uint64_t ring_ctx_sqe_pop(ring_ctx_t* ctx, ring_ctx_notify_ctx_t* notify)
{
    if (atomic_load(&ctx->irps->pool.used) == 1)
    {
        ctx->process = REF(process_current());
    }

    ring_t* ring = &ctx->ring;
    uint32_t stail = atomic_load_explicit(&ring->ctrl->stail, memory_order_acquire);
    uint32_t shead = atomic_load_explicit(&ring->ctrl->shead, memory_order_relaxed);

    if (shead == stail)
    {
        errno = EAGAIN;
        return ERR;
    }

    sqe_t* sqe = &ring->squeue[shead & ring->smask];
    irp_t* irp = irp_new(ctx->irps, sqe);
    if (irp == NULL)
    {
        return ERR;
    }

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

uint64_t ring_ctx_notify(ring_ctx_t* ctx, size_t amount, size_t wait)
{
    if (amount == 0)
    {
        return 0;
    }

    if (ring_ctx_acquire(ctx) == ERR)
    {
        errno = EBUSY;
        return ERR;
    }

    if (!(atomic_load(&ctx->flags) & RING_CTX_MAPPED))
    {
        ring_ctx_release(ctx);
        errno = EINVAL;
        return ERR;
    }

    size_t processed = 0;

    ring_ctx_notify_ctx_t notify = {
        .irps = LIST_CREATE(notify.irps),
        .link = NULL,
    };

    while (processed < amount)
    {
        if (ring_ctx_sqe_pop(ctx, &notify) == ERR)
        {
            break;
        }
        processed++;
    }

    while (!list_is_empty(&notify.irps))
    {
        irp_t* irp = CONTAINER_OF(list_pop_front(&notify.irps), irp_t, entry);
        ring_ctx_dispatch(irp);
    }

    if (wait == 0)
    {
        ring_ctx_release(ctx);
        return processed;
    }

    if (WAIT_BLOCK(&ctx->waitQueue, ring_ctx_avail_cqes(ctx) >= wait) == ERR)
    {
        ring_ctx_release(ctx);
        return processed > 0 ? processed : ERR;
    }

    ring_ctx_release(ctx);
    return processed;
}

SYSCALL_DEFINE(SYS_SETUP, ring_id_t, ring_t* userRing, void* address, size_t sentries, size_t centries)
{
    if (userRing == NULL || sentries == 0 || centries == 0 || !IS_POW2(sentries) || !IS_POW2(centries))
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = process_current();
    space_t* space = &process->space;

    ring_ctx_t* ctx = NULL;
    ring_id_t id = 0;
    for (id = 0; id < ARRAY_SIZE(process->rings); id++)
    {
        ring_ctx_flags_t expected = RING_CTX_NONE;
        if (atomic_compare_exchange_strong(&process->rings[id].flags, &expected, RING_CTX_BUSY))
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

    if (ring_ctx_map(ctx, space, id, userRing, address, sentries, centries) == ERR)
    {
        ring_ctx_release(ctx);
        return ERR;
    }

    ring_ctx_release(ctx);
    return id;
}

SYSCALL_DEFINE(SYS_TEARDOWN, uint64_t, ring_id_t id)
{
    process_t* process = process_current();
    if (id >= ARRAY_SIZE(process->rings))
    {
        errno = EINVAL;
        return ERR;
    }

    ring_ctx_t* ctx = &process->rings[id];
    if (ring_ctx_acquire(ctx) == ERR)
    {
        errno = EBUSY;
        return ERR;
    }

    if (!(atomic_load(&ctx->flags) & RING_CTX_MAPPED))
    {
        ring_ctx_release(ctx);
        errno = EINVAL;
        return ERR;
    }

    if (ctx->irps != NULL && atomic_load(&ctx->irps->pool.used) != 0)
    {
        ring_ctx_release(ctx);
        errno = EBUSY;
        return ERR;
    }

    if (ring_ctx_unmap(ctx) == ERR)
    {
        ring_ctx_release(ctx);
        return ERR;
    }

    ring_ctx_release(ctx);
    return 0;
}

SYSCALL_DEFINE(SYS_ENTER, uint64_t, ring_id_t id, size_t amount, size_t wait)
{
    process_t* process = process_current();
    if (id >= ARRAY_SIZE(process->rings))
    {
        errno = EINVAL;
        return ERR;
    }

    ring_ctx_t* ctx = &process->rings[id];
    return ring_ctx_notify(ctx, amount, wait);
}