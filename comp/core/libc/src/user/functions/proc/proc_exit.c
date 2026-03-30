#include <libc/proc.h>

#include "user/common/exit_stack.h"
#include "user/common/file.h"

extern void _fini(void); // Defined in the C runtime library

void proc_exit(const char* result)
{
    _exit_stack_dispatch();
    _files_close();
    _fini();
    syscall1(SYS_PROC_EXIT, NULL, (uintptr_t)result);
    __builtin_unreachable();
}