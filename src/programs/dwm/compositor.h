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
} compositor_ctx_t;

void compositor_init(void);

void compositor_redraw(compositor_ctx_t* ctx);

void compositor_set_redraw_needed(void);

bool compositor_redraw_needed(void);
