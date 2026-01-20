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
#include <kernel/sync/async.h>
#include <kernel/sync/irp.h>

#include <errno.h>
#include <sys/list.h>
#include <sys/rings.h>
#include <time.h>

static inline uint64_t async_acquire(async_t* ctx)
{
    async_flags_t expected = atomic_load(&ctx->flags);
    if (!(expected & ASYNC_BUSY) && atomic_compare_exchange_strong(&ctx->flags, &expected, expected | ASYNC_BUSY))
    {
        return 0;
    }

    return ERR;
}

static inline void async_release(async_t* ctx)
{
    atomic_fetch_and(&ctx->flags, ~ASYNC_BUSY);
}

static inline uint64_t async_map(async_t* ctx, space_t* space, rings_id_t id, rings_t* userRings, void* address,
    size_t sentries, size_t centries)
{
    rings_t* kernelRings = &ctx->rings;

    size_t pageAmount =
        BYTES_TO_PAGES(sizeof(rings_shared_t) + (sentries * sizeof(sqe_t)) + (centries * sizeof(cqe_t)));
    if (pageAmount >= CONFIG_MAX_ASYNC_PAGES)
    {
        errno = ENOMEM;
        return ERR;
    }

    if (centries >= POOL_IDX_MAX)
    {
        errno = EINVAL;
        return ERR;
    }

    pfn_t pages[CONFIG_MAX_ASYNC_PAGES];
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

    rings_shared_t* shared = (rings_shared_t*)kernelAddr;
    atomic_init(&shared->shead, 0);
    atomic_init(&shared->stail, 0);
    atomic_init(&shared->ctail, 0);
    atomic_init(&shared->chead, 0);
    for (size_t i = 0; i < SEQ_REGS_MAX; i++)
    {
        atomic_init(&shared->regs[i], 0);
    }

    userRings->shared = userAddr;
    userRings->id = id;
    userRings->squeue = (sqe_t*)((uintptr_t)userAddr + sizeof(rings_shared_t));
    userRings->sentries = sentries;
    userRings->smask = sentries - 1;
    userRings->cqueue = (cqe_t*)((uintptr_t)userAddr + sizeof(rings_shared_t) + (sentries * sizeof(sqe_t)));
    userRings->centries = centries;
    userRings->cmask = centries - 1;

    kernelRings->shared = kernelAddr;
    kernelRings->id = id;
    kernelRings->squeue = (sqe_t*)((uintptr_t)kernelAddr + sizeof(rings_shared_t));
    kernelRings->sentries = sentries;
    kernelRings->smask = sentries - 1;
    kernelRings->cqueue = (cqe_t*)((uintptr_t)kernelAddr + sizeof(rings_shared_t) + (sentries * sizeof(sqe_t)));
    kernelRings->centries = centries;
    kernelRings->cmask = centries - 1;

    ctx->irps = irps;
    ctx->descs = descs;
    ctx->userAddr = userAddr;
    ctx->kernelAddr = kernelAddr;
    ctx->pageAmount = pageAmount;
    ctx->space = space;

    atomic_fetch_or(&ctx->flags, ASYNC_MAPPED);
    return 0;
}

static inline uint64_t async_unmap(async_t* ctx)
{
    irp_pool_free(ctx->irps);
    ctx->irps = NULL;

    mem_desc_pool_free(ctx->descs);
    ctx->descs = NULL;

    vmm_unmap(ctx->space, ctx->userAddr, ctx->pageAmount * PAGE_SIZE);
    vmm_unmap(NULL, ctx->kernelAddr, ctx->pageAmount * PAGE_SIZE);

    atomic_fetch_and(&ctx->flags, ~ASYNC_MAPPED);
    return 0;
}

static inline uint64_t async_avail_cqes(async_t* ctx)
{
    rings_t* rings = &ctx->rings;
    uint32_t ctail = atomic_load_explicit(&rings->shared->ctail, memory_order_relaxed);
    uint32_t chead = atomic_load_explicit(&rings->shared->chead, memory_order_acquire);
    return ctail - chead;
}

void async_init(async_t* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->rings = (rings_t){0};
    ctx->irps = NULL;
    ctx->userAddr = NULL;
    ctx->kernelAddr = NULL;
    ctx->pageAmount = 0;
    ctx->space = NULL;
    wait_queue_init(&ctx->waitQueue);
    atomic_init(&ctx->flags, ASYNC_NONE);
}

void async_deinit(async_t* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (async_acquire(ctx) == ERR)
    {
        panic(NULL, "failed to acquire async context for deinitialization");
    }

    if (atomic_load(&ctx->flags) & ASYNC_MAPPED)
    {
        if (async_unmap(ctx) == ERR)
        {
            panic(NULL, "failed to deinitialize async context");
        }
    }

    async_release(ctx);
    wait_queue_deinit(&ctx->waitQueue);
}

static void async_dispatch(irp_t* irp);

static void async_complete(irp_t* irp, void* _ptr)
{
    UNUSED(_ptr);

    async_t* ctx = irp_get_ctx(irp);

    sqe_flags_t reg = (irp->flags >> SQE_SAVE) & SQE_REG_MASK;
    if (reg != SQE_REG_NONE)
    {
        atomic_store_explicit(&ctx->rings.shared->regs[reg], irp->result, memory_order_release);
    }

    rings_t* rings = &ctx->rings;
    uint32_t tail = atomic_load_explicit(&rings->shared->ctail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&rings->shared->chead, memory_order_acquire);

    if ((tail - head) >= rings->centries)
    {
        /// @todo Handle overflow properly.
        panic(NULL, "Async completion queue overflow");
    }

    cqe_t* cqe = &rings->cqueue[tail & rings->cmask];
    cqe->verb = irp->verb;
    cqe->error = irp->err;
    cqe->data = irp->data;
    cqe->_result = irp->result;

    atomic_store_explicit(&rings->shared->ctail, tail + 1, memory_order_release);
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
            async_dispatch(next);
        }
    }

    irp_free(irp);

    if (atomic_load(&ctx->irps->pool.used) == 0)
    {
        UNREF(ctx->process);
        ctx->process = NULL;
    }
}

static void async_dispatch(irp_t* irp)
{
    async_t* ctx = irp_get_ctx(irp);

    for (uint64_t i = 0; i < SEQ_MAX_ARGS; i++)
    {
        sqe_flags_t reg = (irp->flags >> (i * SQE_REG_SHIFT)) & SQE_REG_MASK;
        if (reg == SQE_REG_NONE)
        {
            continue;
        }

        irp->sqe._args[i] = atomic_load_explicit(&ctx->rings.shared->regs[reg], memory_order_acquire);
    }

    irp_push(irp, async_complete, NULL);
    irp_dispatch(irp);
}

typedef struct
{
    list_t irps;
    irp_t* link;
} async_notify_ctx_t;

static uint64_t async_sqe_pop(async_t* ctx, async_notify_ctx_t* notify)
{
    if (atomic_load(&ctx->irps->pool.used) == 1)
    {
        process_t* process = process_current();
        assert(&process->async[0] <= ctx && ctx <= &process->async[CONFIG_MAX_ASYNC_RINGS - 1]);
        ctx->process = REF(process);
    }

    rings_t* rings = &ctx->rings;
    uint32_t stail = atomic_load_explicit(&rings->shared->stail, memory_order_acquire);
    uint32_t shead = atomic_load_explicit(&rings->shared->shead, memory_order_relaxed);

    if (shead == stail)
    {
        errno = EAGAIN;
        return ERR;
    }

    sqe_t* sqe = &rings->squeue[shead & rings->smask];
    irp_t* irp = irp_new(ctx->irps, sqe);
    if (irp == NULL)
    {
        return ERR;
    }

    atomic_store_explicit(&rings->shared->shead, shead + 1, memory_order_release);

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

uint64_t async_notify(async_t* ctx, size_t amount, size_t wait)
{
    if (amount == 0)
    {
        return 0;
    }

    if (async_acquire(ctx) == ERR)
    {
        errno = EBUSY;
        return ERR;
    }

    if (!(atomic_load(&ctx->flags) & ASYNC_MAPPED))
    {
        async_release(ctx);
        errno = EINVAL;
        return ERR;
    }

    size_t processed = 0;

    async_notify_ctx_t notify = {
        .irps = LIST_CREATE(notify.irps),
        .link = NULL,
    };

    while (processed < amount)
    {
        if (async_sqe_pop(ctx, &notify) == ERR)
        {
            break;
        }
        processed++;
    }

    while (!list_is_empty(&notify.irps))
    {
        irp_t* irp = CONTAINER_OF(list_pop_front(&notify.irps), irp_t, entry);
        async_dispatch(irp);
    }

    if (wait == 0)
    {
        async_release(ctx);
        return processed;
    }

    if (WAIT_BLOCK(&ctx->waitQueue, async_avail_cqes(ctx) >= wait) == ERR)
    {
        async_release(ctx);
        return processed > 0 ? processed : ERR;
    }

    async_release(ctx);
    return processed;
}

SYSCALL_DEFINE(SYS_SETUP, rings_id_t, rings_t* userRings, void* address, size_t sentries, size_t centries)
{
    if (userRings == NULL || sentries == 0 || centries == 0 || !IS_POW2(sentries) || !IS_POW2(centries))
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = process_current();
    space_t* space = &process->space;

    async_t* ctx = NULL;
    rings_id_t id = 0;
    for (id = 0; id < CONFIG_MAX_ASYNC_RINGS; id++)
    {
        async_flags_t expected = ASYNC_NONE;
        if (atomic_compare_exchange_strong(&process->async[id].flags, &expected, ASYNC_BUSY))
        {
            ctx = &process->async[id];
            break;
        }
    }

    if (ctx == NULL)
    {
        errno = EMFILE;
        return ERR;
    }

    if (async_map(ctx, space, id, userRings, address, sentries, centries) == ERR)
    {
        async_release(ctx);
        return ERR;
    }

    async_release(ctx);
    return id;
}

SYSCALL_DEFINE(SYS_TEARDOWN, uint64_t, rings_id_t id)
{
    if (id >= CONFIG_MAX_ASYNC_RINGS)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = process_current();
    async_t* ctx = &process->async[id];

    if (async_acquire(ctx) == ERR)
    {
        errno = EBUSY;
        return ERR;
    }

    if (!(atomic_load(&ctx->flags) & ASYNC_MAPPED))
    {
        async_release(ctx);
        errno = EINVAL;
        return ERR;
    }

    if (ctx->irps != NULL && atomic_load(&ctx->irps->pool.used) != 0)
    {
        async_release(ctx);
        errno = EBUSY;
        return ERR;
    }

    if (async_unmap(ctx) == ERR)
    {
        async_release(ctx);
        return ERR;
    }

    async_release(ctx);
    return 0;
}

SYSCALL_DEFINE(SYS_ENTER, uint64_t, rings_id_t id, size_t amount, size_t wait)
{
    if (id >= CONFIG_MAX_ASYNC_RINGS)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = process_current();
    async_t* ctx = &process->async[id];

    return async_notify(ctx, amount, wait);
}