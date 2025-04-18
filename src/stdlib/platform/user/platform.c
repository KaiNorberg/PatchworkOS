#include "../platform.h"
#include "common/print.h"
#include "common/thread.h"

#include <sys/io.h>
#include <sys/proc.h>

void _PlatformInit(void)
{
}

void* _PlatformPageAlloc(uint64_t amount)
{
    return _SyscallVirtualAlloc(NULL, amount * PAGE_SIZE, PROT_READ | PROT_WRITE);
}

int* _PlatformErrnoFunc(void)
{
    return &_ThreadById(_SyscallThreadId())->err;
}

static void _PlatformVprintfPutFunc(char chr, void* context)
{
    write(STDOUT_FILENO, &chr, 1);
}

// TODO: Implement streams!
int _PlatformVprintf(const char* _RESTRICT format, va_list args)
{
    return _Print(_PlatformVprintfPutFunc, NULL, format, args);
}
