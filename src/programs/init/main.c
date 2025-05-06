#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>

// TODO: Add config file to specify programs to start att boot.

void spawn_program(const char* path)
{
    fd_t klog = open("sys:/klog");
    const char* argv[] = {path, NULL};
    spawn_fd_t fds[] = {{.parent = klog, .child = STDOUT_FILENO}, SPAWN_FD_END};
    spawn(argv, fds);
    close(klog);
}

int main(void)
{
    chdir("home:/usr");

    spawn_program("home:/bin/dwm");

    stat_t info;
    while (stat("sys:/net/local/listen/dwm", &info) == ERR)
    {
        thrd_yield();
    }

    spawn_program("home:/bin/wall");
    spawn_program("home:/bin/cursor");
    spawn_program("home:/bin/taskbar");

    // spawn_program("home:/bin/cursor");
    // spawn_program("home:/bin/taskbar");

    return 0;
}
