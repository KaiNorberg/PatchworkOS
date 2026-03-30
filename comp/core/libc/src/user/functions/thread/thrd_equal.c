#include <libc/proc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <threads.h>

#include "user/common/threading.h"

int thrd_equal(thrd_t lhs, thrd_t rhs)
{
    return (lhs == rhs);
}
