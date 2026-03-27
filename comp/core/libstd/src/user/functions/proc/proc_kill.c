#include <libstd/fs.h>
#include <libstd/io.h>
#include <libstd/proc.h>

status_t proc_kill(proc_t pid)
{
    return iostorep(IOPATH(IOFMT("/proc/%d/ctl", pid)), "kill");
}