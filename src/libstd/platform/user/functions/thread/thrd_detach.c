#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

int thrd_detach(thrd_t thr)
{
    // TODO: Implement this
    return thrd_error;
}
