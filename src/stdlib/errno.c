#ifndef __EMBED__

#include <errno.h>

#include "internal/thrd.h"

int* _ErrnoFunc(void)
{
    return &_ThrdBlockById(gettid())->err;
}

#endif
