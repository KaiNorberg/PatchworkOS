#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

int mtx_init(mtx_t* mutex, int type)
{
    atomic_init(&mutex->state, FUTEX_UNLOCKED);
    mutex->owner = ERR;
    mutex->depth = 0;

    return thrd_success;
}
