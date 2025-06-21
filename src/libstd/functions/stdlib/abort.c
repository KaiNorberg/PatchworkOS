#include <stdlib.h>

#include "platform/platform.h"

void abort(void)
{
    _platform_abort("libstd abort");
}
