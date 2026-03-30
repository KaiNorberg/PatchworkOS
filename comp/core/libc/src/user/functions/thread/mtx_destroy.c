#include <libc/proc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <threads.h>

#include "user/common/threading.h"

void mtx_destroy(mtx_t* mutex)
{
    UNUSED(mutex);
    // Do nothing
}
