#include "surface.h"

#include "client.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>

static surface_id_t newId = 0;

surface_t* surface_new(client_t* client, const char* name, const point_t* point, uint64_t width, uint64_t height,
    surface_type_t type)
{
    surface_t* surface = malloc(sizeof(surface_t));
    if (surface == NULL)
    {
        printf("dwm surface error: failed to allocate surface\n");
        errno = ENOMEM;
        return NULL;
    }

    list_entry_init(&surface->dwmEntry);
    list_entry_init(&surface->clientEntry);
    surface->client = client;
    surface->pos = *point;
    surface->shmem = open("/dev/shmem/new");
    if (surface->shmem == ERR)
    {
        free(surface);
        printf("dwm surface error: failed to open shmem\n");
        return NULL;
    }
    surface->buffer = mmap(surface->shmem, NULL, width * height * sizeof(pixel_t), PROT_READ | PROT_WRITE);
    if (surface->buffer == NULL)
    {
        close(surface->shmem);
        free(surface);
        printf("dwm surface error: failed to allocate gfx buffer\n");
        return NULL;
    }
    memset(surface->buffer, 0, width * height * sizeof(pixel_t));
    surface->width = width;
    surface->height = height;
    surface->invalidRect = RECT_INIT_DIM(0, 0, width, height);
    surface->prevRect = RECT_INIT_DIM(surface->pos.x, surface->pos.y, width, height);
    surface->id = newId++;
    surface->type = type;
    surface->timer.flags = TIMER_NONE;
    surface->timer.timeout = CLOCKS_NEVER;
    surface->timer.deadline = CLOCKS_NEVER;
    surface->flags = 0;
    strcpy(surface->name, name);
    return surface;
}

void surface_free(surface_t* surface)
{
    munmap(surface->buffer, surface->width * surface->height * sizeof(pixel_t));
    free(surface);
}

void surface_get_info(surface_t* surface, surface_info_t* info)
{
    info->type = surface->type;
    info->id = surface->id;
    info->rect = SURFACE_SCREEN_RECT(surface);
    info->flags = surface->flags & ~(SURFACE_INVALID | SURFACE_MOVED);
    strcpy(info->name, surface->name);
}

void surface_transfer(surface_t* dest, const surface_t* src, const rect_t* destRect, const point_t* srcPoint)
{
    int64_t width = RECT_WIDTH(destRect);
    int64_t height = RECT_HEIGHT(destRect);

    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (srcPoint->x < 0 || srcPoint->y < 0 || srcPoint->x + width > src->width || srcPoint->y + height > src->height)
    {
        return;
    }
    if (destRect->left < 0 || destRect->top < 0 || destRect->left + width > dest->width ||
        destRect->top + height > dest->height)
    {
        return;
    }

    if (dest == src)
    {
        for (int64_t y = 0; y < RECT_HEIGHT(destRect); y++)
        {
            memmove(&dest->buffer[destRect->left + (y + destRect->top) * dest->width],
                &src->buffer[srcPoint->x + (y + srcPoint->y) * src->width], RECT_WIDTH(destRect) * sizeof(pixel_t));
        }
    }
    else
    {
        for (int64_t y = 0; y < RECT_HEIGHT(destRect); y++)
        {
            memcpy(&dest->buffer[destRect->left + (y + destRect->top) * dest->width],
                &src->buffer[srcPoint->x + (y + srcPoint->y) * src->width], RECT_WIDTH(destRect) * sizeof(pixel_t));
        }
    }

    surface_invalidate(dest, destRect);
}

void surface_transfer_blend(surface_t* dest, const surface_t* src, const rect_t* destRect, const point_t* srcPoint)
{
    int64_t width = RECT_WIDTH(destRect);
    int64_t height = RECT_HEIGHT(destRect);

    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (srcPoint->x < 0 || srcPoint->y < 0 || srcPoint->x + width > src->width || srcPoint->y + height > src->height)
    {
        return;
    }
    if (destRect->left < 0 || destRect->top < 0 || destRect->left + width > dest->width ||
        destRect->top + height > dest->height)
    {
        return;
    }

    for (int64_t y = 0; y < RECT_HEIGHT(destRect); y++)
    {
        for (int64_t x = 0; x < RECT_WIDTH(destRect); x++)
        {
            pixel_t pixel = src->buffer[(srcPoint->x + x) + (srcPoint->y + y) * src->width];
            pixel_t* out = &dest->buffer[(destRect->left + x) + (destRect->top + y) * dest->width];
            PIXEL_BLEND(out, &pixel);
        }
    }

    surface_invalidate(dest, destRect);
}

void surface_invalidate(surface_t* gfx, const rect_t* rect)
{
    if (RECT_AREA(&gfx->invalidRect) == 0)
    {
        gfx->invalidRect = *rect;
    }
    else
    {
        RECT_EXPAND_TO_CONTAIN(&gfx->invalidRect, rect);
    }
}
