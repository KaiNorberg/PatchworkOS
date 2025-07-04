#include "platform/platform.h"
#include "common/exit_stack.h"
#include "common/print.h"
#include "common/std_streams.h"
#include "common/syscalls.h"
#include "common/thread.h"
#include "platform/user/common/heap.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>

static void _populate_std_descriptors(void)
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

void _platform_early_init(void)
{
    _threading_init();
    _populate_std_descriptors();
    _exit_stack_init();
    _files_init();
    _std_streams_init();
    _heap_init();
}

void _platform_late_init(void)
{
}

int* _platform_errno_get(void)
{
    static int garbageErrno;
    _thread_t* thread = _thread_get(gettid());
    if (thread == NULL)
    {
        return &garbageErrno;
    }
    return &thread->err;
}

// Ignore message
void _platform_abort(const char* message)
{
    // raise( SIGABRT ); // TODO: Implement signals
    exit(EXIT_FAILURE);
}
