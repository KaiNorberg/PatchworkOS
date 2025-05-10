#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

void thrd_exit(int res)
{
    _Thread_t* thread = _ThreadById(_SyscallThreadId());
    thread->result = res;
    atomic_store(&thread->running, false);
    futex(&thread->running, FUTEX_ALL, FUTEX_WAKE, CLOCKS_NEVER);
    _ThreadUnref(thread);

    _ThreadUnref(thread); // Dereference base reference
    _SyscallThreadExit();
}
