#include <string.h>
#include <sys/io.h>
#include <stdio.h>
#include <errno.h>

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
    if (writef(ctl, "bind testserver") == ERR)
    {
        printf("error: bind (%s)\n", strerror(errno));
    }
    if (writef(ctl, "listen") == ERR)
    {
        printf("error: listen (%s)\n", strerror(errno));
    }

    fd_t conn = openf("sys:/net/local/%s/data", id);
    if (conn == ERR)
    {
        printf("error: conn open (%s)\n", strerror(errno));
    }
    char buffer[MAX_PATH];
    uint64_t count = read(conn, buffer, MAX_PATH - 1);
    if (count == ERR)
    {
        printf("error: conn read (%s)\n", strerror(errno));
    }
    buffer[count] = '\0';
    printf(buffer);
    close(conn);

    close(ctl);
    close(handle);

    return 0;
}
