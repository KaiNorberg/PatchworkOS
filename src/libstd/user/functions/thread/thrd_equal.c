#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "user/common/syscalls.h"
#include "user/common/thread.h"

int thrd_equal(thrd_t lhs, thrd_t rhs)
{
    return (lhs.id == rhs.id);
}
