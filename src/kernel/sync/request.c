#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/request.h>

#include <kernel/cpu/percpu.h>

typedef struct request_ctx
{
    list_t timeouts;
    lock_t lock;
} request_ctx_t;

PERCPU_DEFINE_CTOR(request_ctx_t, pcpu_requests)
{
    request_ctx_t* ctx = SELF_PTR(pcpu_requests);

    list_init(&ctx->timeouts);
    lock_init(&ctx->lock);
}

request_pool_t* request_pool_new(size_t size, void* ctx)
{
    if (size == 0 || size >= REQUEST_ID_MAX)
    {
        errno = EINVAL;
        return NULL;
    }

    request_pool_t* pool = malloc(sizeof(request_pool_t) + (sizeof(request_t) * size));
    if (pool == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    pool->ctx = ctx;
    pool->used = 0;
    list_init(&pool->free);
    for (request_id_t i = 0; i < (request_id_t)size; i++)
    {
        request_t* request = &pool->requests[i];

        list_entry_init(&request->entry);
        list_entry_init(&request->timeoutEntry);
        request->complete = NULL;
        request->cancel = NULL;
        request->deadline = CLOCKS_NEVER;
        request->index = i;
        request->next = REQUEST_ID_MAX;
        request->cpu = CPU_ID_INVALID;
        request->flags = 0;
        request->type = 0;
        request->err = 0;
        request->data = NULL;
        request->result = 0;
        for (size_t j = 0; j < ARRAY_SIZE(request->_raw); j++)
        {
            request->_raw[j] = 0;
        }

        list_push_back(&pool->free, &pool->requests[i].entry);
    }

    return pool;
}

void request_pool_free(request_pool_t* pool)
{
    free(pool);
}

void request_timeout_add(request_t* request)
{
    request_ctx_t* ctx = SELF_PTR(pcpu_requests);
    LOCK_SCOPE(&ctx->lock);

    request->cpu = SELF->id;

    clock_t now = clock_uptime();
    request->deadline = CLOCKS_DEADLINE(request->timeout, now);

    request_t* entry;
    LIST_FOR_EACH(entry, &ctx->timeouts, timeoutEntry)
    {
        if (request->deadline < entry->deadline)
        {
            list_prepend(&entry->entry, &request->timeoutEntry);
            timer_set(now, request->deadline);
            return;
        }
    }

    list_push_back(&ctx->timeouts, &request->timeoutEntry);
    timer_set(now, request->deadline);
}

void request_timeout_remove(request_t* request)
{
    request_ctx_t* ctx = CPU_PTR(request->cpu, pcpu_requests);
    assert(ctx != NULL);

    LOCK_SCOPE(&ctx->lock);
    list_remove(&request->timeoutEntry);
}

void request_timeouts_check(void)
{
    request_ctx_t* ctx = SELF_PTR(pcpu_requests);
    assert(ctx != NULL);

    clock_t now = clock_uptime();

    lock_acquire(&ctx->lock);

    request_t* request;
    while (true)
    {
        request = CONTAINER_OF_SAFE(list_first(&ctx->timeouts), request_t, timeoutEntry);
        if (request == NULL)
        {
            break;
        }

        if (request->deadline > now)
        {
            timer_set(now, request->deadline);
            break;
        }

        list_remove(&request->timeoutEntry);
        lock_release(&ctx->lock);

        assert(request->cancel != NULL);
        request->err = ETIMEDOUT;
        request->cancel(request);

        lock_acquire(&ctx->lock);
    }

    lock_release(&ctx->lock);
}