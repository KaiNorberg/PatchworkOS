#include "platform/platform.h"

#include "common/thread.h"

#include <errno.h>

int* _ErrnoFunc(void)
{
    return _PlatformErrnoFunc();
}