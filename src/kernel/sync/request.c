#include <kernel/sched/clock.h>
#include <kernel/sync/request.h>

#include <kernel/cpu/percpu.h>

PERCPU_DEFINE_CTOR(request_ctx_t, pcpu_requests)
{
    request_ctx_t* requests = SELF_PTR(pcpu_requests);

    list_init(&requests->timeouts);
    lock_init(&requests->lock);
}

void request_timeout_add(request_t* request)
{
    request_ctx_t* requests = SELF_PTR(pcpu_requests);
    LOCK_SCOPE(&requests->lock);

    request->ctx = requests;

    request_t* entry;
    LIST_FOR_EACH(entry, &requests->timeouts, timeoutEntry)
    {
        if (request->deadline < entry->deadline)
        {
            list_prepend(&entry->entry, &request->timeoutEntry);
            return;
        }
    }

    list_push_back(&requests->timeouts, &request->timeoutEntry);
}

void request_timeout_remove(request_t* request)
{
    assert(request->owner != NULL);
    LOCK_SCOPE(&request->ctx->lock);

    list_remove(&request->timeoutEntry);
}

void request_timeouts_check(void)
{
    request_ctx_t* requests = SELF_PTR(pcpu_requests);
    clock_t now = clock_uptime();

    LOCK_SCOPE(&requests->lock);

    request_t* request;
    while (true)
    {
        request = CONTAINER_OF(list_first(&requests->timeouts), request_t, timeoutEntry);
        if (request == NULL)
        {
            break;
        }

        if (request->deadline > now)
        {
            break;
        }

        list_remove(&request->timeoutEntry);
        /// @todo 
    }
}