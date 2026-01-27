#include <sys/fs.h>
#include <sys/proc.h>

status_t kill(pid_t pid)
{
    return writefiles(F("/proc/%llu/ctl", pid), "kill");
}