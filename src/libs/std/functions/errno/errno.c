#include <errno.h>

//Thread specific storage not implemented at kernel level
static int _errno = 0;

int* _ErrnoFunc()
{
    return &_errno;
}