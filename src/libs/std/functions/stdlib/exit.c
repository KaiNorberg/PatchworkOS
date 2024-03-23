#include <stdlib.h>

#include "internal/syscalls/syscalls.h"

_NORETURN void exit(int status)
{
    _ProcessExit(status);
}