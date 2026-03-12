#include <sys/fs.h>
#include <sys/io.h>
#include <sys/proc.h>

status_t proc_kill(proc_t pid)
{
    return iostorep(IOPATH(IOFMT("/proc/%d/ctl", pid)), "kill");
}