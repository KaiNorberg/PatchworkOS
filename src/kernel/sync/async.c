#include <kernel/mem/paging_types.h>
#include <kernel/mem/vmm.h>
#include <kernel/mem/pmm.h>
#include <kernel/proc/process.h>
#include <kernel/sync/async.h>
#include <kernel/cpu/syscall.h>
#include <kernel/log/panic.h>
#include <kernel/sync/tasks.h>

#include <errno.h>
#include <sys/async.h>
#include <sys/list.h>

static inline uint64_t async_ctx_map(async_ctx_t* ctx, space_t* space, async_rings_t* userRings, void* address, size_t sentries, size_t centries)
{
    async_rings_t* kernelRings = &ctx->rings;

    size_t pageAmount = BYTES_TO_PAGES(sizeof(async_shared_t) + (sentries * sizeof(async_sqe_t)) + (centries * sizeof(async_cqe_t)));
    if (pageAmount >= CONFIG_MAX_ASYNC_PAGES)
    {
        errno = ENOMEM;
        return ERR;
    }

    void* pages[CONFIG_MAX_ASYNC_PAGES];
    if (pmm_alloc_pages(pages, pageAmount) == ERR)
    {
        errno = ENOMEM;
        return ERR;
    }

    for (size_t i = 0; i < pageAmount; i++)
    {
        memset(pages[i], 0, PAGE_SIZE);
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

    task_t* tasks = malloc(sizeof(task_t) * centries);
    if (tasks == NULL)
    {
        vmm_unmap(space, userAddr, pageAmount * PAGE_SIZE);
        vmm_unmap(NULL, kernelAddr, pageAmount * PAGE_SIZE);
        return ERR;
    }

    for (size_t i = 0; i < centries; i++)
    {
        TASK_INIT(&tasks[i]);
        list_push_back(&ctx->freeTasks, &tasks[i].entry);
    }

    async_shared_t* shared = (async_shared_t*)kernelAddr;
    atomic_init(&shared->shead, 0);
    atomic_init(&shared->stail, 0);
    atomic_init(&shared->ctail, 0);
    atomic_init(&shared->chead, 0);

    userRings->shared = userAddr;
    userRings->squeue = (async_sqe_t*)((uintptr_t)userAddr + sizeof(async_shared_t));
    userRings->sentries = sentries;
    userRings->smask = sentries - 1;
    userRings->cqueue = (async_cqe_t*)((uintptr_t)userAddr + sizeof(async_shared_t) + (sentries * sizeof(async_sqe_t)));
    userRings->centries = centries;
    userRings->cmask = centries - 1;

    kernelRings->shared = kernelAddr;
    kernelRings->squeue = (async_sqe_t*)((uintptr_t)kernelAddr + sizeof(async_shared_t));
    kernelRings->sentries = sentries;
    kernelRings->smask = sentries - 1;
    kernelRings->cqueue = (async_cqe_t*)((uintptr_t)kernelAddr + sizeof(async_shared_t) + (sentries * sizeof(async_sqe_t)));
    kernelRings->centries = centries;    
    kernelRings->cmask = centries - 1;

    ctx->tasks = tasks;
    ctx->userAddr = userAddr;
    ctx->kernelAddr = kernelAddr;
    ctx->pageAmount = pageAmount;
    ctx->space = space;

    atomic_fetch_or(&ctx->flags, ASYNC_CTX_MAPPED);
    return 0;
}

static inline uint64_t async_ctx_unmap(async_ctx_t* ctx)
{
    list_init(&ctx->freeTasks);
    free(ctx->tasks);
    ctx->tasks = NULL;

    vmm_unmap(ctx->space, ctx->userAddr, ctx->pageAmount * PAGE_SIZE);
    vmm_unmap(NULL, ctx->kernelAddr, ctx->pageAmount * PAGE_SIZE);

    atomic_fetch_and(&ctx->flags, ~ASYNC_CTX_MAPPED);
    return 0;
}

static inline task_t* async_ctx_alloc_task(async_ctx_t* ctx)
{
    if (list_is_empty(&ctx->freeTasks))
    {
        return NULL;
    }

    return CONTAINER_OF(list_pop_back(&ctx->freeTasks), task_t, entry);
}

static inline void async_ctx_free_task(async_ctx_t* ctx, task_t* task)
{
    list_push_back(&ctx->freeTasks, &task->entry);
}

void async_ctx_init(async_ctx_t* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(ctx, 0, sizeof(async_ctx_t));
    list_init(&ctx->freeTasks);
    wait_queue_init(&ctx->waitQueue);
    atomic_init(&ctx->flags, ASYNC_CTX_NONE);
}

void async_ctx_deinit(async_ctx_t* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (async_ctx_acquire(ctx) == ERR)
    {
        panic(NULL, "failed to acquire async context for deinitialization");
    }

    if (atomic_load(&ctx->flags) & ASYNC_CTX_MAPPED)
    {
        if (async_ctx_unmap(ctx) == ERR)
        {
            panic(NULL, "failed to deinitialize async context");
        }
    }

    async_ctx_release(ctx);
    wait_queue_deinit(&ctx->waitQueue);
}

static void async_nop_complete(task_nop_t* nop)
{
    async_cqe_t cqe;
    cqe.data = nop->data;
    cqe.opcode = ASYNC_OP_NOP;
    cqe.error = EOK;
    
    process_t* process = nop->process;
    async_ctx_push_cqe(&process->async, &cqe);
    async_ctx_free_task(&process->async, (task_t*)nop);
    UNREF(nop->process);
}

static uint64_t async_handle_sqe(async_ctx_t* ctx, async_sqe_t* sqe)
{
    task_t* task = async_ctx_alloc_task(ctx);
    if (task == NULL)
    {
        errno = ENOSPC;
        return ERR;
    }

    switch (sqe->opcode)
    {
    case ASYNC_OP_NOP:
    {
        task_nop_t* nop = (task_nop_t*)task;
        nop->data = sqe->data;
        nop->process = REF(process_current());
        nop->complete = async_nop_complete;
        nop->cancel = task_nop_cancel;
        nop->timeout = task_nop_timeout;
        TASK_DELAY_NO_QUEUE(nop);
    }
    break;
    default:
        async_ctx_free_task(ctx, task);
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

uint64_t async_ctx_notify(async_ctx_t* ctx, size_t amount, size_t wait)
{
    if (amount == 0)
    {
        return 0;
    }

    if (async_ctx_acquire(ctx) == ERR)
    {
        errno = EBUSY;
        return ERR;
    }

    if (!(atomic_load(&ctx->flags) & ASYNC_CTX_MAPPED))
    {
        async_ctx_release(ctx);
        errno = EINVAL;
        return ERR;
    }

    async_rings_t* rings = &ctx->rings;
    size_t processed = 0;

    while (processed < amount)
    {
        uint32_t stail = atomic_load_explicit(&rings->shared->stail, memory_order_acquire);
        uint32_t shead = atomic_load_explicit(&rings->shared->shead, memory_order_relaxed);

        if (shead == stail)
        {
            break;
        }

        async_sqe_t sqe = rings->squeue[shead & rings->smask];
        atomic_store_explicit(&rings->shared->shead, shead + 1, memory_order_release);

        if (async_handle_sqe(ctx, &sqe) == ERR)
        {
            async_ctx_release(ctx);
            return ERR;
        }
        processed++;
    }

    if (wait == 0)
    {
        async_ctx_release(ctx);
        return processed;
    }

    while (true)
    {
        uint32_t ctail = atomic_load_explicit(&rings->shared->ctail, memory_order_relaxed);
        uint32_t chead = atomic_load_explicit(&rings->shared->chead, memory_order_acquire);

        if ((ctail - chead) >= wait)
        {
            break;
        }

        if (WAIT_BLOCK(&ctx->waitQueue, false) == ERR)
        {
            async_ctx_release(ctx);
            return processed > 0 ? processed : ERR;
        }
    }

    async_ctx_release(ctx);
    return processed;
}

SYSCALL_DEFINE(SYS_ASYNC_INIT, uint64_t, async_rings_t* userRings, void* address, size_t sentries, size_t centries)
{
    if (userRings == NULL || sentries == 0 || centries == 0 || !IS_POW2(sentries) || !IS_POW2(centries))
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = process_current();
    async_ctx_t* ctx = &process->async;
    space_t* space = &process->space;
    
    if (async_ctx_acquire(ctx) == ERR)
    {
        errno = EBUSY;
        return ERR;
    }

    if (atomic_load(&ctx->flags) & ASYNC_CTX_MAPPED)
    {
        async_ctx_release(ctx);
        errno = EBUSY;
        return ERR;
    }

    if (async_ctx_map(ctx, space, userRings, address, sentries, centries) == ERR)
    {
        async_ctx_release(ctx);
        return ERR;
    }

    async_ctx_release(ctx);
    return 0;
}

SYSCALL_DEFINE(SYS_ASYNC_DEINIT, uint64_t)
{
    process_t* process = process_current();
    async_ctx_t* ctx = &process->async;
    space_t* space = &process->space;

    if (async_ctx_acquire(ctx) == ERR)
    {
        errno = EBUSY;
        return ERR;
    }

    if (!(atomic_load(&ctx->flags) & ASYNC_CTX_MAPPED))
    {
        async_ctx_release(ctx);
        errno = EINVAL;
        return ERR;
    }

    if (async_ctx_unmap(ctx) == ERR)
    {
        async_ctx_release(ctx);
        return ERR;
    }

    async_ctx_release(ctx);
    return 0;
}

SYSCALL_DEFINE(SYS_ASYNC_NOTIFY, uint64_t, size_t amount, size_t wait)
{
    process_t* process = process_current();
    async_ctx_t* ctx = &process->async;

    return async_ctx_notify(ctx, amount, wait);
}