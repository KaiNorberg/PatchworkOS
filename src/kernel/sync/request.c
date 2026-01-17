#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/request.h>

#include <kernel/cpu/percpu.h>

PERCPU_DEFINE_CTOR(request_ctx_t, pcpu_requests)
{
    request_ctx_t* ctx = SELF_PTR(pcpu_requests);

    list_init(&ctx->timeouts);
    lock_init(&ctx->lock);
}

void request_timeout_add(request_t* request)
{
    request_ctx_t* ctx = SELF_PTR(pcpu_requests);
    LOCK_SCOPE(&ctx->lock);

    request->ctx = ctx;

    request_t* entry;
    LIST_FOR_EACH(entry, &ctx->timeouts, timeoutEntry)
    {
        if (request->deadline < entry->deadline)
        {
            list_prepend(&entry->entry, &request->timeoutEntry);
            return;
        }
    }

    list_push_back(&ctx->timeouts, &request->timeoutEntry);

    timer_set(clock_uptime(), request->deadline);
}

void request_timeout_remove(request_t* request)
{
    assert(request->ctx != NULL);
    LOCK_SCOPE(&request->ctx->lock);

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

        assert(request->timeout != NULL);
        request->timeout(request);

        lock_acquire(&ctx->lock);
    }

    lock_release(&ctx->lock);
}