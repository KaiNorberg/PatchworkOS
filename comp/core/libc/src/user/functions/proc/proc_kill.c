#include <libc/fs.h>
#include <libc/io.h>
#include <libc/proc.h>

status_t proc_kill(proc_t pid)
{
    return iostorep(IOPATH(IOFMT("/proc/%d/ctl", pid)), "kill");
}