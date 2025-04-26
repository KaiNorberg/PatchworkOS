#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/io.h>

int main(void)
{
    fd_t handle = open("sys:/net/local/new");
    char id[MAX_PATH] = {0};
    if (read(handle, id, MAX_PATH) == ERR)
    {
        printf("error: id read (%s)\n", strerror(errno));
    }
    printf("id: %s\n", id);

    fd_t ctl = openf("sys:/net/local/%s/ctl", id);
    if (ctl == ERR)
    {
        printf("error: ctl open (%s)\n", strerror(errno));
    }

    if (writef(ctl, "connect testserver") == ERR)
    {
        printf("error: connect (%s)\n", strerror(errno));
    }

    fd_t data = openf("sys:/net/local/%s/data", id);
    if (data == ERR)
    {
        printf("error: data open (%s)\n", strerror(errno));
    }
    if (writef(data, "Hello, World!") == ERR)
    {
        printf("error: data write (%s)\n", strerror(errno));
    }

    close(data);
    close(ctl);

    return 0;
}
