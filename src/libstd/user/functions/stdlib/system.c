#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/io.h>
#include <sys/proc.h>

int system(const char* command)
{
    const char* argv[] = {"/bin/shell", command, NULL};
    proc_t shell;
    status_t status = proc_create(argv, PROC_DEFAULT, &shell);
    if (IS_ERR(status))
    {
        return -1;
    }

    fd_t wait;
    status = open(&wait, IOFMT("/proc/%d/wait", shell));
    if (IS_ERR(status))
    {
        return -1;
    }

    char buf[MAX_PATH];
    status = ioread(wait, buf, MAX_PATH, IOCUR, NULL);
    if (IS_ERR(status))
    {
        close(wait);
        return -1;
    }

    close(wait);
    return atoi(buf);
}
