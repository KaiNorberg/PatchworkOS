#include <stdlib.h>

#include "internal/syscalls/syscalls.h"

__attribute__((__noreturn__)) void exit(int status)
{
    sys_exit_process(status);
}