#include "screen.h"

#include "region.h"
#include "surface.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>

static uint64_t width;
static uint64_t height;
static uint64_t pitch;
static uint64_t stride;
static char format[MAX_NAME];

static void* frontbuffer;
static void* backbuffer;

static rect_t screenRect;
static region_t invalidRegion;

static void frontbuffer_init(void)
{
    char name[MAX_PATH] = {0};
    if (readfile("/dev/fb/0/name", name, sizeof(name) - 1, 0) == ERR)
    {
        printf("dwm: failed to read framebuffer name (%s)\n", strerror(errno));
        abort();
    }

    if (scanfile("/dev/fb/0/info", "%lu %lu %lu %s", &width, &height, &pitch, format) == ERR)
    {
        printf("dwm: failed to read framebuffer info (%s)\n", strerror(errno));
        abort();
    }

    printf("dwm: using framebuffer '%s' width=%lu height=%lu pitch=%lu format=%s\n", name, width, height, pitch,
        format);

    stride = pitch / sizeof(pixel_t);

    fd_t data = open("/dev/fb/0/data");
    if (data == ERR)
    {
        printf("dwm: failed to open framebuffer device (%s)\n", strerror(errno));
        abort();
    }

    frontbuffer = mmap(data, NULL, height * pitch, PROT_READ | PROT_WRITE);
    if (frontbuffer == NULL)
    {
        printf("dwm: failed to map framebuffer memory (%s)\n", strerror(errno));
        abort();
    }
    memset(frontbuffer, 0, height * pitch);

    close(data);
}

static void backbuffer_init(void)
{
    backbuffer = malloc(height * pitch);
    if (backbuffer == NULL)
    {
        printf("dwm: failed to allocate backbuffer memory\n");
        abort();
    }
}

static void screen_invalidate(const rect_t* rect)
{
    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &screenRect);
    region_add(&invalidRegion, &fitRect);
}

void screen_init(void)
{
    frontbuffer_init();
    backbuffer_init();
    screenRect = RECT_INIT_DIM(0, 0, width, height);
    region_init(&invalidRegion);
}

void screen_deinit(void)
{
    free(backbuffer);
    munmap(frontbuffer, height * pitch);
}

void screen_transfer(surface_t* surface, const rect_t* rect)
{
    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &screenRect);

    point_t srcPoint = {
        .x = MAX(fitRect.left - surface->pos.x, 0),
        .y = MAX(fitRect.top - surface->pos.y, 0),
    };
    int64_t width = RECT_WIDTH(&fitRect);
    int64_t height = RECT_HEIGHT(&fitRect);
    for (int64_t y = 0; y < height; y++)
    {
        memcpy(&((pixel_t*)backbuffer)[(fitRect.left) + (fitRect.top + y) * stride],
            &surface->buffer[(srcPoint.x) + (srcPoint.y + y) * surface->width], width * sizeof(pixel_t));
    }
    screen_invalidate(rect);
}

void screen_transfer_blend(surface_t* surface, const rect_t* rect)
{
    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &screenRect);

    point_t srcPoint = {
        .x = MAX(fitRect.left - surface->pos.x, 0),
        .y = MAX(fitRect.top - surface->pos.y, 0),
    };
    int64_t width = RECT_WIDTH(&fitRect);
    int64_t height = RECT_HEIGHT(&fitRect);
    for (int64_t y = 0; y < height; y++)
    {
        for (int64_t x = 0; x < width; x++)
        {
            pixel_t* pixel = &surface->buffer[(srcPoint.x + x) + (srcPoint.y + y) * surface->width];
            pixel_t* out = &((pixel_t*)backbuffer)[(fitRect.left + x) + (fitRect.top + y) * stride];
            PIXEL_BLEND(out, pixel);
        }
    }
    screen_invalidate(&fitRect);
}

void screen_transfer_frontbuffer(surface_t* surface, const rect_t* rect)
{
    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &screenRect);

    point_t srcPoint = {
        .x = MAX(fitRect.left - surface->pos.x, 0),
        .y = MAX(fitRect.top - surface->pos.y, 0),
    };
    for (int64_t y = 0; y < RECT_HEIGHT(&fitRect); y++)
    {
        memcpy(&((uint32_t*)frontbuffer)[(fitRect.left) + (fitRect.top + y) * stride],
            &surface->buffer[(srcPoint.x) + (srcPoint.y + y) * surface->width],
            RECT_WIDTH(&fitRect) * sizeof(uint32_t));
    }

    region_clear(&invalidRegion);
}

void screen_swap(void)
{
    for (uint64_t i = 0; i < invalidRegion.count; i++)
    {
        rect_t* rect = &invalidRegion.rects[i];
        for (int64_t y = 0; y < RECT_HEIGHT(rect); y++)
        {
            memcpy(&((uint32_t*)frontbuffer)[(rect->left) + (rect->top + y) * stride],
                &((pixel_t*)backbuffer)[(rect->left) + (rect->top + y) * stride], RECT_WIDTH(rect) * sizeof(uint32_t));
        }
    }

    region_clear(&invalidRegion);
}

uint64_t screen_width(void)
{
    return width;
}

uint64_t screen_height(void)
{
    return height;
}

void screen_rect(rect_t* rect)
{
    *rect = screenRect;
}
