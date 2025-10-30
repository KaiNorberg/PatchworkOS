#include "common/use_annex_k.h"
#include <stdio.h>
#include <stdlib.h>

void abort_handler_s(const char* _RESTRICT msg, void* _RESTRICT ptr, errno_t err)
{
#ifdef _KERNEL_
    (void)ptr;
    (void)err;
    (void)msg;
#else
    (void)ptr;
    (void)err;
    fprintf(stderr, "abort handler called:\n%s\n", msg);
#endif
    abort();
}
