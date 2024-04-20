#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <sys/io.h>
#include <sys/process.h>
#include <sys/ioctl.h>

#include <libs/std/internal/syscalls/syscalls.h>

#define BUFFER_SIZE 0x10000
char buffer[BUFFER_SIZE];

int main(void)
{
    fd_t fd = open("A:/framebuffer/0");

    memset(buffer, INT32_MAX, BUFFER_SIZE);
    write(fd, buffer, BUFFER_SIZE);

    close(fd);

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
