#pragma once

#include <libpatchwork/point.h>
#include <libpatchwork/rect.h>
#include <libpatchwork/surface.h>
#include <sys/io.h>
#include <sys/list.h>

#include "gfx.h"

typedef struct client client_t;

typedef struct timer
{
    timer_flags_t flags;
    clock_t timeout;
    clock_t deadline;
} timer_t;

typedef struct surface
{
    list_entry_t dwmEntry;
    list_entry_t clientEntry;
    client_t* client;
    point_t pos;
    fd_t shmem;
    gfx_t gfx;
    surface_id_t id;
    surface_type_t type;
    timer_t timer;
    bool isInvalid;
    bool hasMoved;
    bool isVisible;
    bool isFocused;
    rect_t prevRect;
    char name[MAX_NAME];
} surface_t;

#define SURFACE_RECT(surface) RECT_INIT_DIM(surface->pos.x, surface->pos.y, surface->gfx.width, surface->gfx.height);

#define SURFACE_CONTENT_RECT(surface) RECT_INIT_DIM(0, 0, surface->gfx.width, surface->gfx.height);

#define SURFACE_INVALID_RECT(surface) \
    RECT_INIT_DIM(surface->pos.x + surface->gfx.invalidRect.left, surface->pos.y + surface->gfx.invalidRect.top, \
        RECT_WIDTH(&surface->gfx.invalidRect), RECT_HEIGHT(&surface->gfx.invalidRect));

surface_t* surface_new(client_t* client, const char* name, const point_t* point, uint64_t width,
    uint64_t height, surface_type_t type);

void surface_free(surface_t* surface);

void surface_get_info(surface_t* surface, surface_info_t* info);
