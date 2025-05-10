#include "platform/platform.h"
#include "common/exit_stack.h"
#include "common/print.h"
#include "common/syscalls.h"
#include "common/thread.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>

static fd_t zeroResource;

void _PlatformEarlyInit(void)
{
    _ThreadingInit();
    _ExitStackInit();

    if (write(STDOUT_FILENO, NULL, 0) == ERR)
    {
        fd_t fd = open("sys:/null");
        if (fd != STDOUT_FILENO)
        {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
    }
    if (read(STDIN_FILENO, NULL, 0) == ERR)
    {
        fd_t fd = open("sys:/null");
        if (fd != STDIN_FILENO)
        {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }
    errno = 0;

    zeroResource = open("sys:/zero");
}

void _PlatformLateInit(void)
{
}

void* _PlatformPageAlloc(uint64_t amount)
{
    return mmap(zeroResource, NULL, amount * PAGE_SIZE, PROT_READ | PROT_WRITE);
}

int* _PlatformErrnoFunc(void)
{
    return &_ThreadById(_SyscallThreadId())->err;
}

// TODO: Implement streams!
int _PlatformVprintf(const char* _RESTRICT format, va_list args)
{
    return vwritef(STDOUT_FILENO, format, args);
}

void _PlatformAbort(void)
{
    _SyscallProcessExit(EXIT_FAILURE);
}
