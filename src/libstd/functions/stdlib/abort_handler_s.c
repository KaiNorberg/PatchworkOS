#include "common/use_annex_k.h"
#include <stdio.h>
#include <stdlib.h>

#include "platform/platform.h"

void abort_handler_s(const char* _RESTRICT msg, void* _RESTRICT ptr, errno_t err)
{
#ifndef __KERNEL__
    (void)ptr;
    (void)err;
    fprintf(stderr, "abort handler called:\n%s\n", msg);
#else
    (void)msg;
    (void)ptr;
    (void)err;
#endif
    _platform_abort("libstd abort_handler_s");
}
