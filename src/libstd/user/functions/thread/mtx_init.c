#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "user/common/syscalls.h"
#include "user/common/thread.h"

int mtx_init(mtx_t* mutex, int type)
{
    UNUSED(type); // We don't care about the type, we just implement all types the same way as the C specifcation says
                  // that, for example, if a non-recursive mutex is locked recursively, the behavior is undefined so we
                  // just say that it works.

    atomic_init(&mutex->state, _MTX_UNLOCKED);
    mutex->owner = ERR;
    mutex->depth = 0;

    return thrd_success;
}
