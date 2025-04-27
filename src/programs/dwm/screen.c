#include "screen.h"

#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/fb.h>
#include <sys/proc.h>

static fb_info_t info;
static uint32_t* frontbuffer;
static uint32_t* backbuffer;

static void screen_frontbuffer_init(void)
{
    fd_t fb = open("sys:/fb0");
    if (fb == ERR)
    {
        exit(EXIT_FAILURE);
    }
    if (ioctl(fb, IOCTL_FB_INFO, &info, sizeof(fb_info_t)) == ERR)
    {
        exit(EXIT_FAILURE);
    }
    if (info.format != FB_ARGB32)
    {
        exit(EXIT_FAILURE);
    }
    frontbuffer = mmap(fb, NULL, info.stride * info.height * sizeof(uint32_t), PROT_READ | PROT_WRITE);
    if (frontbuffer == NULL)
    {
        exit(EXIT_FAILURE);
    }
    close(fb);
}

void screen_init(void)
{
    screen_frontbuffer_init();
    backbuffer = malloc(info.stride * info.height * sizeof(uint32_t));
    if (backbuffer == NULL)
    {
        exit(EXIT_FAILURE);
    }

    memset(frontbuffer, 0, info.stride * info.height * sizeof(uint32_t));
    memset(backbuffer, 0, info.width * info.height * sizeof(uint32_t));
}

void screen_deinit(void)
{
    free(backbuffer);
    munmap(frontbuffer, info.stride * info.height * sizeof(uint32_t));
}
