#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <sys/io.h>
#include <sys/mem.h>
#include <sys/process.h>
#include <sys/ioctl.h>

#include <libs/std/internal/syscalls.h>

#define FB_ADDR ((void*)0xF0000000)

//TODO: Implement ioctl to get size
#define FB_SIZE (1920 * 1080 * sizeof(uint32_t))

int main(void)
{
    *((uint64_t*)main) = *((uint64_t*)main);

    fd_t fd = open("A:/framebuffer/0");

    if (mmap(fd, FB_ADDR, FB_SIZE, PROT_READ | PROT_WRITE) == NULL)
    {
        SYSCALL(SYS_TEST, 1, strerror(errno));
        return EXIT_FAILURE;
    }

    //mprotect((void*)((uint64_t)FB_ADDR + FB_SIZE / 2), 0x1000, PROT_READ);
    //munmap((void*)((uint64_t)FB_ADDR + FB_SIZE / 2), 0x1000);

    memset(FB_ADDR, -1, FB_SIZE);

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
