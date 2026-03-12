#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/io.h>
#include <sys/proc.h>

int system(const char* command)
{
    proc_t shell;
    status_t status = proc_create(PROC_ARGS("/bin/shell", command), NULL, 0, PRIO_DEFAULT, PROC_DEFAULT, &shell);
    if (IS_ERR(status))
    {
        return -1;
    }

    char buf[MAX_PATH] = {0};
    status = ioreadp(IOPATH(IOFMT("/proc/%d/wait", shell)), IOBUF(buf, sizeof(buf) - 1), IOCUR, NULL);
    if (IS_ERR(status))
    {
        proc_kill(shell);
        return -1;
    }

    return atoi(buf);
}
