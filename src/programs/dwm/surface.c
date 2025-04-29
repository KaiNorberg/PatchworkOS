#include "surface.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/list.h>

surface_t* surface_new(client_t* client, surface_id_t id, const point_t* point, uint64_t width, uint64_t height, surface_type_t type)
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

void surface_get_non_panel_rect(surface_t* surface, const rect_t* rect)
{
    /*rect_t newRect = RECT_INIT_DIM(0, 0, backbuffer.width, backbuffer.height);

    window_t* window;
    LIST_FOR_EACH(window, &windows, entry)
    {
        window->moved = true;

        if (window->type != DWM_PANEL)
        {
            continue;
        }

        uint64_t leftDist = window->pos.x + window->gfx.width;
        uint64_t topDist = window->pos.y + window->gfx.height;
        uint64_t rightDist = backbuffer.width - window->pos.x;
        uint64_t bottomDist = backbuffer.height - window->pos.y;

        if (leftDist <= topDist && leftDist <= rightDist && leftDist <= bottomDist)
        {
            newRect.left = MAX(window->pos.x + window->gfx.width, newRect.left);
        }
        else if (topDist <= leftDist && topDist <= rightDist && topDist <= bottomDist)
        {
            newRect.top = MAX(window->pos.y + window->gfx.height, newRect.top);
        }
        else if (rightDist <= leftDist && rightDist <= topDist && rightDist <= bottomDist)
        {
            newRect.right = MIN(window->pos.x, newRect.right);
        }
        else if (bottomDist <= leftDist && bottomDist <= topDist && bottomDist <= rightDist)
        {
            newRect.bottom = MIN(window->pos.y, newRect.bottom);
        }
    }

    clientRect = newRect;*/
}
