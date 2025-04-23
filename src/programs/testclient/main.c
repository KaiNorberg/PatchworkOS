#include <sys/io.h>

int main(void)
{
    fd_t new = open("sys:/net/local/new");
    char id[MAX_PATH];
    read(new, id, MAX_PATH);
    close(new);

    fd_t ctl = openf("sys:/net/local/%d/ctl", id);
    writef(ctl, "connect testserver");

    fd_t data = openf("sys:/net/local/%d/data", id);
    writef(data, "Hello, World!");

    close(data);
    close(ctl);

    return 0;
}
