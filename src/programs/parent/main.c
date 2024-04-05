#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <sys/io.h>
#include <sys/process.h>
#include <sys/ioctl.h>

#include <libs/std/internal/syscalls/syscalls.h>

int main(void)
{
    if (spawn("child.elf") == ERR)
    {
        SYSCALL(SYS_TEST, 1, strerror(errno));
        return EXIT_FAILURE;
    }

    while (1)
    {
        SYSCALL(SYS_TEST, 1, "Hello from parent!");
    }

    return EXIT_SUCCESS;
}
