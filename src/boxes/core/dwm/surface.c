#include "surface.h"

#include "client.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
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
    if (surface->shmem == _FAIL)
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
    info->flags = surface->flags;
    strcpy(info->name, surface->name);
}
