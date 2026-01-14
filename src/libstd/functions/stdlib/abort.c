#include <signal.h>
#include <stdlib.h>
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
    proc_exit("aborted");
#endif
}
