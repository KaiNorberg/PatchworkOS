#include <errno.h>

#include "platform/platform.h"

int* _ErrnoFunc(void)
{
    return _PlatformErrnoFunc();
}
