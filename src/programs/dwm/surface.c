#include "surface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>

surface_t* surface_new(client_t* client, surface_id_t id, const point_t* point, uint64_t width, uint64_t height,
    surface_type_t type)
{
    surface_t* surface = malloc(sizeof(surface_t));
    if (surface == NULL)
    {
        printf("dwm surface error: failed to allocate surface\n");
        return NULL;
    }

    list_entry_init(&surface->dwmEntry);
    list_entry_init(&surface->clientEntry);
    surface->client = client;
    surface->pos = *point;

    fd_t shmem = open("sys:/shmem/new");
    if (shmem == ERR)
    {
        free(surface);
        printf("dwm surface error: failed to open shmem\n");
        return NULL;
    }
    surface->shmem[read(shmem, surface->shmem, MAX_NAME)] = '\0';
    surface->gfx.buffer = mmap(shmem, NULL, width * height * sizeof(pixel_t), PROT_READ | PROT_WRITE);
    close(shmem);

    if (surface->gfx.buffer == NULL)
    {
        free(surface);
        printf("dwm surface error: failed to allocate gfx buffer\n");
        return NULL;
    }

    memset(surface->gfx.buffer, 0, width * height * sizeof(pixel_t));
    surface->gfx.width = width;
    surface->gfx.height = height;
    surface->gfx.stride = width;
    surface->gfx.invalidRect = RECT_INIT_DIM(0, 0, width, height);
    surface->id = id;
    surface->type = type;
    surface->timer.flags = TIMER_NONE;
    surface->timer.timeout = CLOCKS_NEVER;
    surface->timer.deadline = CLOCKS_NEVER;
    surface->invalid = true;
    surface->moved = false;
    surface->prevRect = RECT_INIT_DIM(surface->pos.x, surface->pos.y, width, height);
    return surface;
}

void surface_free(surface_t* surface)
{
    munmap(surface->gfx.buffer, surface->gfx.width * surface->gfx.height * sizeof(pixel_t));
    free(surface);
}