#include "screen.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/fb.h>
#include <sys/io.h>
#include <sys/proc.h>

static fb_info_t info;
static void* frontbuffer;

static gfx_t backbuffer;

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

    switch (info.format)
    {
    case FB_ARGB32:
    {
        frontbuffer = mmap(fb, NULL, info.stride * info.height * sizeof(uint32_t), PROT_READ | PROT_WRITE);
        if (frontbuffer == NULL)
        {
            exit(EXIT_FAILURE);
        }
        memset(frontbuffer, 0, info.stride * info.height * sizeof(uint32_t));
    }
    break;
    default:
    {
        exit(EXIT_FAILURE);
    }
    }

    close(fb);
}

static void screen_backbuffer_init(void)
{
    backbuffer.buffer = malloc(info.stride * info.height * sizeof(pixel_t));
    if (backbuffer.buffer == NULL)
    {
        exit(EXIT_FAILURE);
    }
    backbuffer.width = info.width;
    backbuffer.height = info.height;
    backbuffer.stride = info.stride;
    backbuffer.invalidRect = (rect_t){0};
    memset(backbuffer.buffer, 0, info.width * info.height * sizeof(pixel_t));
}

void screen_init(void)
{
    screen_frontbuffer_init();
    screen_backbuffer_init();
}

void screen_deinit(void)
{
    free(backbuffer.buffer);
    munmap(frontbuffer, info.stride * info.height * sizeof(uint32_t));
}

void screen_transfer(surface_t* surface, const rect_t* destRect, const point_t* srcPoint)
{
    gfx_transfer(&backbuffer, &surface->gfx, destRect, srcPoint);
}

void screen_swap(void)
{
    switch (info.format)
    {
    case FB_ARGB32:
    {
        gfx_t gfx;
        gfx.buffer = frontbuffer;
        gfx.width = info.width;
        gfx.height = info.height;
        gfx.stride = info.stride;
        gfx_swap(&gfx, &backbuffer, &backbuffer.invalidRect);
    }
    break;
    default:
    {
        exit(EXIT_FAILURE);
    }
    }
    backbuffer.invalidRect = (rect_t){0};
}

uint64_t screen_width(void)
{
    return info.width;
}

uint64_t screen_height(void)
{
    return info.height;
}

void screen_rect(rect_t* rect)
{
    *rect = RECT_INIT_DIM(0, 0, info.width, info.height);
}
