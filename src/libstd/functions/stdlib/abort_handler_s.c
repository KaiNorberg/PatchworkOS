#include "common/use_annex_k.h"
#include <stdio.h>
#include <stdlib.h>

#include "platform/platform.h"

void abort_handler_s(const char* _RESTRICT msg, void* _RESTRICT ptr, errno_t err)
{
    // TODO: Implement fprintf
    // fprintf(stderr, "abort handler called:\n%s\n", msg);
    _PlatformAbort();
}
