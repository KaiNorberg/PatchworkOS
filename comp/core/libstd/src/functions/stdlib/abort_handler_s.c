#include "common/use_annex_k.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/defs.h>

void abort_handler_s(const char* _RESTRICT msg, void* _RESTRICT ptr, errno_t err)
{
#ifdef _KERNEL_
    UNUSED(ptr);
    UNUSED(err);
    UNUSED(msg);
#else
    UNUSED(ptr);
    UNUSED(err);
    fprintf(stderr, "abort handler called:\n%s\n", msg);
#endif
    abort();
}
