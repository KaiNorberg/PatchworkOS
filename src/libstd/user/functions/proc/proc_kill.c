#include <sys/fs.h>
#include <sys/proc.h>

status_t proc_kill(proc_t pid)
{
    return writefiles(F("/proc/%llu/ctl", pid), "kill");
}