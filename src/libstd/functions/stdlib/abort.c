#include <stdlib.h>

#ifdef __KERNEL__
#include "log/panic.h"
#endif

void abort(void)
{
#ifdef __KERNEL__
    panic(NULL, "abort() called");
#else
    // TODO: Implement signals
    exit(EXIT_FAILURE);
#endif
}
