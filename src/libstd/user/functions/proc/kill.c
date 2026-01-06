#include <sys/io.h>
#include <sys/proc.h>

uint64_t kill(pid_t pid)
{
    return swritefile(F("/proc/%llu/ctl", pid), "kill");
}