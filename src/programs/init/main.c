#include <sys/io.h>
#include <sys/proc.h>

void spawn_program(const char* path)
{
    const char* argv[] = {path, NULL};
    spawn(argv, NULL);
}

int main(void)
{
    chdir("home:/usr");

    spawn_program("home:/bin/wall");
    spawn_program("home:/bin/cursor");
    spawn_program("home:/bin/taskbar");

    return 0;
}
