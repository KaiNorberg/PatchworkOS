#pragma once

#include <libdwm/point.h>
#include <libdwm/rect.h>
#include <libdwm/surface.h>
#include <sys/io.h>
#include <sys/list.h>

#include "gfx.h"

typedef struct client client_t;

typedef struct surface
{
    list_entry_t dwmEntry;
    list_entry_t clientEntry;
    client_t* client;
    point_t pos;
    gfx_t gfx;
    surface_id_t id;
    surface_type_t type;
    bool invalid;
    bool moved;
    rect_t prevRect;
} surface_t;

#define SURFACE_RECT(surface) RECT_INIT_DIM(surface->pos.x, surface->pos.y, surface->gfx.width, surface->gfx.height);

#define SURFACE_CONTENT_RECT(surface) RECT_INIT_DIM(0, 0, surface->gfx.width, surface->gfx.height);

#define SURFACE_INVALID_RECT(surface) \
    RECT_INIT_DIM(surface->pos.x + surface->gfx.invalidRect.left, surface->pos.y + surface->gfx.invalidRect.top, \
        RECT_WIDTH(&surface->gfx.invalidRect), RECT_HEIGHT(&surface->gfx.invalidRect));

surface_t* surface_new(client_t* client, surface_id_t id, const point_t* point, uint64_t width, uint64_t height,
    surface_type_t type);

void surface_free(surface_t* surface);

void surface_get_non_panel_rect(surface_t* surface, const rect_t* rect);
