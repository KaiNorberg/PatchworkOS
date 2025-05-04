#include "surface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

surface_t* surface_new(client_t* client, surface_id_t id, const point_t* point, uint64_t width, uint64_t height,
    surface_type_t type)
{
    surface_t* surface = malloc(sizeof(surface_t));
    if (surface == NULL)
    {
        printf("dwm surface error: failed to allocate surface");
        return NULL;
    }

    list_entry_init(&surface->dwmEntry);
    list_entry_init(&surface->clientEntry);
    surface->client = client;
    surface->pos = *point;
    surface->gfx.buffer = malloc(width * height * sizeof(pixel_t));
    if (surface->gfx.buffer == NULL)
    {
        free(surface);
        printf("dwm surface error: failed to allocate gfx buffer");
        return NULL;
    }
    memset(surface->gfx.buffer, 0, width * height * sizeof(pixel_t));
    surface->gfx.width = width;
    surface->gfx.height = height;
    surface->gfx.stride = width;
    surface->gfx.invalidRect = RECT_INIT_DIM(0, 0, width, height);
    surface->id = id;
    surface->type = type;
    surface->invalid = true;
    surface->moved = false;
    surface->prevRect = RECT_INIT_DIM(surface->pos.x, surface->pos.y, width, height);
    return surface;
}

void surface_free(surface_t* surface)
{
    free(surface->gfx.buffer);
    free(surface);
}

uint64_t surface_resize_buffer(surface_t* surface, uint64_t width, uint64_t height)
{
    void* newBuffer = realloc(surface->gfx.buffer, width * height * sizeof(pixel_t));
    if (newBuffer == NULL)
    {
        return ERR;
    }

    surface->gfx.width = width;
    surface->gfx.height = height;
    surface->gfx.stride = width;
    surface->gfx.buffer = newBuffer;
    return 0;
}
