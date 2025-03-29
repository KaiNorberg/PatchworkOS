#include <sys/proc.h>

int main(void)
{
    const char* argv[] = {"home:/bin/shell", NULL};
    spawn(argv, NULL);
}