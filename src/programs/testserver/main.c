#include <sys/io.h>
#include <stdio.h>

int main(void)
{
    fd_t new = open("sys:/net/local/new");
    char id[MAX_PATH] = {0};
    read(new, id, MAX_PATH);
    close(new);
    printf("id: %s\n", id);

    fd_t ctl = openf("sys:/net/local/%d/ctl", id);
    writef(ctl, "bind testserver");

    fd_t conn = openf("sys:/net/local/%d/data", id);

    char buffer[MAX_PATH];
    uint64_t count = read(conn, buffer, MAX_PATH - 1);
    buffer[count] = '\0';
    printf(buffer);

    close(conn);
    close(ctl);

    return 0;
}
