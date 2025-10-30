#include <stdlib.h>

#ifdef _KERNEL_
#include <kernel/log/panic.h>
#endif

void abort(void)
{
#ifdef _KERNEL_
    panic(NULL, "abort() called");
#else
    // TODO: Implement signals
    exit(EXIT_FAILURE);
#endif
}
