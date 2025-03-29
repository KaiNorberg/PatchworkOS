#include <sys/io.h>
#include <sys/proc.h>

int main(void)
{
    chdir("home:/");

    const char* argv[] = {"home:/bin/shell", NULL};
    spawn(argv, NULL);

    return 0;
}