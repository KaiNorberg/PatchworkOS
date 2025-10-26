#include "libstd/_internal/init.h"

#include "constraint_handler.h"
#include "heap.h"
#include "time_utils.h"

#ifndef __KERNEL__
#include "user/user.h"
#endif

#include <sys/io.h>

void _std_init(void)
{
#ifndef __KERNEL__
    _user_init();
#endif

    _heap_init();
    _constraint_handler_init();
    _time_zone_init();
}
