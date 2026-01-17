#include <kernel/cpu/syscall.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sync/async.h>
#include <kernel/sync/request.h>
#include <kernel/sync/requests.h>

#include <errno.h>
#include <sys/list.h>
#include <sys/rings.h>
#include <time.h>

static inline uint64_t async_ctx_acquire(async_ctx_t* ctx)
{
    async_ctx_flags_t expected = atomic_load(&ctx->flags);
    if (!(expected & ASYNC_CTX_BUSY) &&
        atomic_compare_exchange_strong(&ctx->flags, &expected, expected | ASYNC_CTX_BUSY))
    {
        return 0;
    }

    return ERR;
}

static inline void async_ctx_release(async_ctx_t* ctx)
{
    atomic_fetch_and(&ctx->flags, ~ASYNC_CTX_BUSY);
}

static inline uint64_t async_ctx_map(async_ctx_t* ctx, space_t* space, rings_id_t id, rings_t* userRings, void* address,
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

    if (centries >= REQUEST_ID_MAX)
    {
        errno = EINVAL;
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

    request_pool_t* requests = request_pool_new(centries, ctx);
    if (requests == NULL)
    {
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

    ctx->requests = requests;
    ctx->userAddr = userAddr;
    ctx->kernelAddr = kernelAddr;
    ctx->pageAmount = pageAmount;
    ctx->space = space;

    atomic_fetch_or(&ctx->flags, ASYNC_CTX_MAPPED);
    return 0;
}

static inline uint64_t async_ctx_unmap(async_ctx_t* ctx)
{
    request_pool_free(ctx->requests);
    ctx->requests = NULL;

    vmm_unmap(ctx->space, ctx->userAddr, ctx->pageAmount * PAGE_SIZE);
    vmm_unmap(NULL, ctx->kernelAddr, ctx->pageAmount * PAGE_SIZE);

    atomic_fetch_and(&ctx->flags, ~ASYNC_CTX_MAPPED);
    return 0;
}

static inline uint64_t async_ctx_avail_cqes(async_ctx_t* ctx)
{
    rings_t* rings = &ctx->rings;
    uint32_t ctail = atomic_load_explicit(&rings->shared->ctail, memory_order_relaxed);
    uint32_t chead = atomic_load_explicit(&rings->shared->chead, memory_order_acquire);
    return ctail - chead;
}

void async_ctx_init(async_ctx_t* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->rings = (rings_t){0};
    ctx->requests = NULL;
    ctx->userAddr = NULL;
    ctx->kernelAddr = NULL;
    ctx->pageAmount = 0;
    ctx->space = NULL;
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

typedef struct
{
    list_t requests;
    request_t* link;
} async_notify_ctx_t;

static void async_dispatch(request_t* request)
{
    async_ctx_t* ctx = request_get_ctx(request);

    for (uint64_t i = 0; i < SEQ_MAX_ARGS; i++)
    {
        seq_regs_t reg = (request->flags >> (i * SQE_REG_SHIFT)) & SQE_REG_MASK;
        if (reg == SQE_REG_NONE)
        {
            continue;
        }

        request->args[i] = atomic_load_explicit(&ctx->rings.shared->regs[reg], memory_order_acquire);
    }

    switch (request->type)
    {
    case RINGS_NOP:
    {
        request_nop_t* nop = (request_nop_t*)request;
        nop->cancel = request_nop_cancel;
        REQUEST_DELAY_NO_QUEUE(nop);
    }
    break;
    default:
        // Impossible due to the check in async_handle_sqe.
        panic(NULL, "Invalid opcode %d", request->type);
        break;
    }
}

static void async_ctx_push_cqe(async_ctx_t* ctx, rings_op_t opcode, errno_t error, void* data, uint64_t result)
{
    rings_t* rings = &ctx->rings;

    uint32_t tail = atomic_load_explicit(&rings->shared->ctail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&rings->shared->chead, memory_order_acquire);

    if ((tail - head) >= rings->centries)
    {
        /// @todo Handle overflow properly.
        panic(NULL, "Async completion queue overflow");
    }

    cqe_t* cqe = &rings->cqueue[tail & rings->cmask];
    cqe->opcode = opcode;
    cqe->error = error;
    cqe->data = data;
    cqe->_raw = result;

    atomic_store_explicit(&rings->shared->ctail, tail + 1, memory_order_release);

    wait_unblock(&ctx->waitQueue, WAIT_ALL, EOK);
}

static void async_request_complete(request_t* request)
{
    async_ctx_t* ctx = request_get_ctx(request);

    seq_regs_t reg = (request->flags >> SQE_SAVE) & SQE_REG_MASK;
    if (reg != SQE_REG_NONE)
    {
        atomic_store_explicit(&ctx->rings.shared->regs[reg], request->result, memory_order_release);
    }

    async_ctx_push_cqe(ctx, request->type, request->err, request->data, request->result);

    request_t* next = request_next(request);
    if (next != NULL)
    {
        async_dispatch(next);
    }

    request_free(request);

    if (ctx->requests->used == 0)
    {
        UNREF(ctx->process);
        ctx->process = NULL;
    }
}

static uint64_t async_handle_sqe(async_ctx_t* ctx, async_notify_ctx_t* notify, sqe_t* sqe)
{
    if (sqe->opcode < RINGS_MIN_OPCODE || sqe->opcode >= RINGS_MAX_OPCODE)
    {
        async_ctx_push_cqe(ctx, sqe->opcode, EINVAL, sqe->data, ERR);
        return 0;
    }

    request_t* request = request_new(ctx->requests);
    if (request == NULL)
    {
        errno = ENOSPC;
        return ERR;
    }

    if (ctx->requests->used == 1)
    {
        process_t* process = process_current();
        assert(&process->async[0] <= ctx && ctx <= &process->async[CONFIG_MAX_ASYNC_RINGS - 1]);
        ctx->process = REF(process);
    }

    request->complete = async_request_complete;
    request->cancel = NULL;
    request->timeout = sqe->timeout;
    request->flags = sqe->flags;
    request->type = sqe->opcode;
    request->err = EOK;
    request->data = sqe->data;
    request->result = 0;

    if (notify->link != NULL)
    {
        notify->link->next = request->index;
        notify->link = NULL;
    }
    else
    {
        list_push_back(&notify->requests, &request->entry);
    }

    if (sqe->flags & SQE_LINK)
    {
        notify->link = request;
    }

    return 0;
}

uint64_t async_ctx_notify(async_ctx_t* ctx, size_t amount, size_t wait)
{
    if (amount == 0)
    {
        return 0;
    }

    /// @todo Implement the register state logic.

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

    rings_t* rings = &ctx->rings;
    size_t processed = 0;

    async_notify_ctx_t notify = {
        .requests = LIST_CREATE(notify.requests),
        .link = NULL,
    };

    while (processed < amount)
    {
        uint32_t stail = atomic_load_explicit(&rings->shared->stail, memory_order_acquire);
        uint32_t shead = atomic_load_explicit(&rings->shared->shead, memory_order_relaxed);

        if (shead == stail)
        {
            break;
        }

        sqe_t sqe = rings->squeue[shead & rings->smask];
        atomic_store_explicit(&rings->shared->shead, shead + 1, memory_order_release);

        if (async_handle_sqe(ctx, &notify, &sqe) == ERR)
        {
            break;
        }
        processed++;
    }

    while (!list_is_empty(&notify.requests))
    {
        request_t* request = CONTAINER_OF(list_pop_front(&notify.requests), request_t, entry);
        async_dispatch(request);
    }

    if (wait == 0)
    {
        async_ctx_release(ctx);
        return processed;
    }

    if (WAIT_BLOCK(&ctx->waitQueue, async_ctx_avail_cqes(ctx) >= wait) == ERR)
    {
        async_ctx_release(ctx);
        return processed > 0 ? processed : ERR;
    }

    async_ctx_release(ctx);
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

    async_ctx_t* ctx = NULL;
    rings_id_t id = 0;
    for (id = 0; id < CONFIG_MAX_ASYNC_RINGS; id++)
    {
        async_ctx_flags_t expected = ASYNC_CTX_NONE;
        if (atomic_compare_exchange_strong(&process->async[id].flags, &expected, ASYNC_CTX_BUSY))
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

    if (async_ctx_map(ctx, space, id, userRings, address, sentries, centries) == ERR)
    {
        async_ctx_release(ctx);
        return ERR;
    }

    async_ctx_release(ctx);
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
    async_ctx_t* ctx = &process->async[id];

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

    if (ctx->requests != NULL && ctx->requests->used > 0)
    {
        async_ctx_release(ctx);
        errno = EBUSY;
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

SYSCALL_DEFINE(SYS_ENTER, uint64_t, rings_id_t id, size_t amount, size_t wait)
{
    if (id >= CONFIG_MAX_ASYNC_RINGS)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = process_current();
    async_ctx_t* ctx = &process->async[id];

    return async_ctx_notify(ctx, amount, wait);
}