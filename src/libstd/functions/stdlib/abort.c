#include <stdlib.h>

#include "platform/platform.h"

void abort(void)
{
    _PlatformAbort("libstd abort");
}
