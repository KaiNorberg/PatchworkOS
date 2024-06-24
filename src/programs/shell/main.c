#include <stdlib.h>
#include <sys/proc.h>

int main(void)
{
    spawn("home:/bin/cursor.elf");
    spawn("home:/bin/taskbar.elf");

    spawn("home:/bin/calculator.elf");

    return EXIT_SUCCESS;
}
