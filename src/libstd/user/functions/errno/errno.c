#include "user/common/threading.h"

#include <errno.h>
#include <sys/proc.h>

int* _errno_get(void)
{
    static int garbage;

    return &_THREAD_SELF->self->err;
}
