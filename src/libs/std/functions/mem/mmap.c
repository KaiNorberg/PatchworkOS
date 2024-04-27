#include <sys/mem.h>

#include "libs/std/internal/syscalls.h"

void* mmap(uint64_t fd, void* address, uint64_t length, uint8_t prot)
{
    void* result = (void*)SYSCALL(SYS_MMAP, 4, fd, address, length, prot);
    if (result == NULL)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}