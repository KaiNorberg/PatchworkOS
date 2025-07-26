#include <stdlib.h>

#include "platform/user/common/exit_stack.h"
#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

void exit(int status)
{
    _exit_stack_dispatch();
    _files_close();
    _syscall_process_exit(status);
}
