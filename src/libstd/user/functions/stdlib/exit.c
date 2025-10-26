#include <stdlib.h>

#include "user/common/exit_stack.h"
#include "user/common/file.h"
#include "user/common/syscalls.h"

void exit(int status)
{
    _exit_stack_dispatch();
    _files_close();
    _syscall_process_exit(status);
}
