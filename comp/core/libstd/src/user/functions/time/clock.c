#include <libstd/syscall.h>
#include <time.h>

clock_t clock(void)
{
    clock_t time;
    syscall0(SYS_CLOCK, &time);
    return time;
}
