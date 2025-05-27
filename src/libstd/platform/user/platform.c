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

static void _PopulateStdDescriptors(void)
{
    for (uint64_t i = 0; i <= STDERR_FILENO; i++)
    {
        if (write(i, NULL, 0) == ERR && errno == EBADF)
        {
            fd_t nullFd = open("sys:/null");
            if (nullFd != i)
            {
                dup2(nullFd, i);
                close(nullFd);
            }
        }
    }
}

void _PlatformEarlyInit(void)
{
    _ThreadingInit();
    _PopulateStdDescriptors();
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
    static int garbageErrno;
    _Thread_t* thread = _ThreadGet(gettid());
    if (thread == NULL)
    {
        return &garbageErrno;
    }
    return &thread->err;
}

// Ignore message
void _PlatformAbort(const char* message)
{
    // raise( SIGABRT );
    exit(EXIT_FAILURE);
}
