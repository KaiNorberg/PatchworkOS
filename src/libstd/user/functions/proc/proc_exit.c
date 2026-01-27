#include <sys/proc.h>

#include "user/common/exit_stack.h"
#include "user/common/file.h"
#include "user/common/syscalls.h"

void exits(const char* status)
{
    _exit_stack_dispatch();
    _files_close();
    syscall1(SYS_EXITS, NULL, (uintptr_t)status);
    __builtin_unreachable();
}