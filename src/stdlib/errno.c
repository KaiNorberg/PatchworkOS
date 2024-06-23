#include <errno.h>

#ifndef __EMBED__

// Thread specific storage not implemented at kernel level
static int _errno = 0;

int* _ErrnoFunc(void)
{
    return &_errno;
}

#endif
