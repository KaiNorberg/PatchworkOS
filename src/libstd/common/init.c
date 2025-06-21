#include "libstd/_internal/init.h"

#include "../platform/platform.h"
#include "constraint_handler.h"
#include "time_utils.h"

#include <errno.h>
#include <sys/io.h>

void _std_init(void)
{
    _platform_early_init();

    _constraint_handler_init();
    _time_zone_init();

    _platform_late_init();
}
