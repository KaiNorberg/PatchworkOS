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

static void rectangle(uint64_t left, uint64_t top, uint64_t right, uint64_t bottom, uint32_t color)
{    
    uint32_t* fb = (uint32_t*)FB_ADDR;

    for (uint64_t y = top; y < bottom; y++) 
    {
        for (uint64_t x = left; x < right; x++) 
        {
            fb[y * 1920 + x] = color;
        }
    }
}

int main(void)
{
    fd_t fd = open("A:/framebuffer/0");

    if (mmap(fd, FB_ADDR, FB_SIZE, PROT_READ | PROT_WRITE) == NULL)
    {
        SYSCALL(SYS_TEST, 1, strerror(errno));
        return EXIT_FAILURE;
    }

    //mprotect((void*)((uint64_t)FB_ADDR + FB_SIZE / 2), 0x1000, PROT_READ);
    //munmap((void*)((uint64_t)FB_ADDR + FB_SIZE / 2), 0x1000);

    close(fd);

    rectangle(100, 100, 300, 200, 0xFF0000);
    rectangle(400, 300, 600, 500, 0x00FF00);
    rectangle(700, 100, 800, 400, 0x0000FF);
    rectangle(1000, 600, 1200, 800, 0xFFFF00);
    rectangle(50, 800, 250, 900, 0xFF00FF);
    rectangle(1200, 50, 1500, 250, 0x00FFFF);
    rectangle(100, 500, 300, 700, 0x880000);
    rectangle(500, 1000, 700, 1050, 0x008800);
    rectangle(1000, 300, 1100, 500, 0x000088);
    rectangle(1400, 700, 1600, 900, 0x888800); 
    rectangle(600, 600, 800, 800, 0x880088);
    
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
