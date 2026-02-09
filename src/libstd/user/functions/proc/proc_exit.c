#include <sys/proc.h>

#include "user/common/exit_stack.h"
#include "user/common/file.h"

void proc_exit(const char* result)
{
    _exit_stack_dispatch();
    _files_close();
    syscall1(SYS_PROC_EXIT, NULL, (uintptr_t)result);
    __builtin_unreachable();
}