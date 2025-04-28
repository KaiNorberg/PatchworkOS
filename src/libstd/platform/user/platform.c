#include "../platform.h"
#include "common/print.h"
#include "common/thread.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/io.h>
#include <sys/proc.h>

static fd_t zeroResource;

void _PlatformInit(void)
{
    zeroResource = open("sys:/zero");
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
