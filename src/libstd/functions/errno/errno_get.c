#include <errno.h>

#include "platform/platform.h"

int* _errno_get(void)
{
    return _platform_errno_get();
}
