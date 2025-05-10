#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

int mtx_init(mtx_t* mutex, int type)
{
    if (type != mtx_plain)
    {
        return thrd_error;
    }

    atomic_init(&mutex->state, FUTEX_UNLOCKED);

    return thrd_success;
}
