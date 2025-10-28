#pragma once

#include "surface.h"

#include <sys/list.h>

typedef struct client client_t;

typedef struct
{
    list_t* windows;
    list_t* panels;
    surface_t* wall;
    surface_t* cursor;
    surface_t* fullscreen;
} compositor_ctx_t;

void compositor_init(void);

void compositor_draw(compositor_ctx_t* ctx);

void compositor_invalidate(const rect_t* rect);
