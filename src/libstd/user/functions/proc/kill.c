#include <sys/proc.h>
#include <sys/io.h>

uint64_t kill(pid_t pid)
{
    return swritefile(F("/proc/%llu/ctl", pid), "kill");
}