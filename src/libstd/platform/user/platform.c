#include "platform/platform.h"
#include "common/exit_stack.h"
#include "common/print.h"
#include "common/std_streams.h"
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
    _FilesInit();
    _StdStreamsInit();

    zeroResource = open("sys:/zero");
    if (zeroResource == ERR)
    {
        exit(EXIT_FAILURE);
    }
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

void _PlatformAbort(void)
{
    // raise( SIGABRT );
    exit(EXIT_FAILURE);
}
