#include <stdlib.h>
#include <signal.h>
#include <sys/proc.h>

#ifdef _KERNEL_
#include <kernel/log/panic.h>
#endif

void abort(void)
{
#ifdef _KERNEL_
    panic(NULL, "abort() called");
#else
    raise(SIGABRT);
    _exit("aborted");
#endif
}
