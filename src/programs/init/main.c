#include <sys/io.h>
#include <sys/proc.h>

void spawn_program(const char* path)
{
    const char* argv[] = {path, NULL};
    process_create(argv, NULL);
}

int main(void)
{
    fd_t cwd = open("sys:/proc/self/cwd");
    writef(cwd, "home:/usr");
    close(cwd);

    spawn_program("home:/bin/wall");
    spawn_program("home:/bin/cursor");
    spawn_program("home:/bin/taskbar");

    return 0;
}
