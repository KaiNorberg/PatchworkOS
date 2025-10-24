#include "screen.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fb.h>
#include <sys/io.h>
#include <sys/proc.h>

static fb_info_t info;
static void* frontbuffer;

static gfx_t backbuffer;

static scanline_t* scanlines;

static void frontbuffer_init(void)
{
    if (readfile("/dev/fb/0/info", &info, sizeof(fb_info_t), 0) == ERR)
    {
        printf("dwm: failed to read framebuffer info (%s)\n", strerror(errno));
        abort();
    }

    printf("dwm: using framebuffer '%s' width=%lu height=%lu stride=%lu format=%u\n", info.name, info.width,
        info.height, info.stride, info.format);

    fd_t fbBuffer = open("/dev/fb/0/buffer");
    if (fbBuffer == ERR)
    {
        printf("dwm: failed to open framebuffer device (%s)\n", strerror(errno));
        abort();
    }

    switch (info.format)
    {
    case FB_ARGB32:
    {
        frontbuffer = mmap(fbBuffer, NULL, info.stride * info.height * sizeof(uint32_t), PROT_READ | PROT_WRITE);
        if (frontbuffer == NULL)
        {
            printf("dwm: failed to map framebuffer memory (%s)\n", strerror(errno));
            abort();
        }
        memset(frontbuffer, 0, info.stride * info.height * sizeof(uint32_t));
    }
    break;
    default:
    {
        printf("dwm: unsupported framebuffer format\n");
        abort();
    }
    }

    close(fbBuffer);
}

static void backbuffer_init(void)
{
    backbuffer.buffer = malloc(info.stride * info.height * sizeof(pixel_t));
    if (backbuffer.buffer == NULL)
    {
        printf("dwm: failed to allocate backbuffer memory\n");
        abort();
    }
    backbuffer.width = info.width;
    backbuffer.height = info.height;
    backbuffer.stride = info.stride;
    backbuffer.invalidRect = (rect_t){0};
    memset(backbuffer.buffer, 0, info.width * info.height * sizeof(pixel_t));
}

static void scanlines_clear(void)
{
    for (uint64_t i = 0; i < info.height; i++)
    {
        scanlines[i].isInvalid = false;
    }
}

static void scanlines_init(void)
{
    scanlines = calloc(info.height, sizeof(scanline_t));
    if (scanlines == NULL)
    {
        printf("dwm: failed to allocate scanlines memory\n");
        abort();
    }
    scanlines_clear();
}

static void scanlines_invalidate(const rect_t* rect)
{
    for (int64_t i = rect->top; i < rect->bottom; i++)
    {
        if (scanlines[i].isInvalid)
        {
            scanlines[i].start = MIN(scanlines[i].start, rect->left);
            scanlines[i].end = MAX(scanlines[i].end, rect->right);
        }
        else
        {
            scanlines[i].isInvalid = true;
            scanlines[i].start = rect->left;
            scanlines[i].end = rect->right;
        }
    }
}

void screen_init(void)
{
    frontbuffer_init();
    backbuffer_init();
    scanlines_init();
}

void screen_deinit(void)
{
    free(scanlines);
    free(backbuffer.buffer);
    munmap(frontbuffer, info.stride * info.height * sizeof(uint32_t));
}

void screen_transfer(surface_t* surface, const rect_t* rect)
{
    point_t srcPoint = {
        .x = MAX(rect->left - surface->pos.x, 0),
        .y = MAX(rect->top - surface->pos.y, 0),
    };
    gfx_transfer(&backbuffer, &surface->gfx, rect, &srcPoint);
    scanlines_invalidate(rect);
}

void screen_transfer_blend(surface_t* surface, const rect_t* rect)
{
    point_t srcPoint = {
        .x = MAX(rect->left - surface->pos.x, 0),
        .y = MAX(rect->top - surface->pos.y, 0),
    };
    gfx_transfer_blend(&backbuffer, &surface->gfx, rect, &srcPoint);
    scanlines_invalidate(rect);
}

void screen_transfer_frontbuffer(surface_t* surface, const rect_t* rect)
{
    point_t srcPoint = {
        .x = MAX(rect->left - surface->pos.x, 0),
        .y = MAX(rect->top - surface->pos.y, 0),
    };
    switch (info.format)
    {
    case FB_ARGB32:
    {
        for (int64_t y = 0; y < RECT_HEIGHT(rect); y++)
        {
            memcpy(&((pixel_t*)frontbuffer)[rect->left + (y + rect->top) * info.stride],
                &surface->gfx.buffer[srcPoint.x + (y + srcPoint.y) * surface->gfx.stride],
                RECT_WIDTH(rect) * sizeof(pixel_t));
        }
    }
    break;
    default:
    {
        printf("dwm: unsupported framebuffer format\n");
        abort();
    }
    }
    scanlines_clear();
}

void screen_swap(void)
{
    switch (info.format)
    {
    case FB_ARGB32:
    {
        for (uint64_t i = 0; i < info.height; i++)
        {
            if (!scanlines[i].isInvalid)
            {
                continue;
            }

            uint64_t offset = (scanlines[i].start * sizeof(uint32_t)) + i * sizeof(uint32_t) * info.stride;
            uint64_t size = (scanlines[i].end - scanlines[i].start) * sizeof(uint32_t);

            memcpy((void*)((uint64_t)frontbuffer + offset), (void*)((uint64_t)backbuffer.buffer + offset), size);
        }
    }
    break;
    default:
    {
        printf("dwm: unsupported framebuffer format\n");
        abort();
    }
    }
    scanlines_clear();
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
