#include <sys/proc.h>

#include <user/common/syscalls.h>

uint64_t notify(note_func_t func)
{
    uint64_t result = _syscall_notify(func);
    if (result == _FAIL)
    {
        errno = _syscall_errno();
    }
    return result;
}