#include "platform/platform.h"
#if _PLATFORM_HAS_SCHEDULING

#include <errno.h>

#include "common/thread.h"

int* _ErrnoFunc(void)
{
    return &_ThreadById(gettid())->err;
}

#endif