#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

int thrd_equal(thrd_t lhs, thrd_t rhs)
{
    return (lhs.id == rhs.id);
}
