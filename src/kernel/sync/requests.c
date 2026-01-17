#include <kernel/sync/requests.h>

bool request_nop_cancel(request_nop_t* request)
{
    REQUEST_COMPLETE(request, 0);
    return true;
}