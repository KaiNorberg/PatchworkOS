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
/*
    fd_t fb0 = open("/dev/fb/0", O_READ | O_WRITE);

    struct ioctl_framebuffer_info info;
    ioctl(fb0, IOCTL_GET_FB_INFO, &info);

    close(fb0);
*/

    if (spawn("B:/programs/child.elf") == ERR)
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
