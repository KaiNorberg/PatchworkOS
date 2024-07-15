#pragma once

#include "defs.h"
#include "lock.h"
#include "msg_queue.h"
#include "vfs.h"

#include <sys/gfx.h>
#include <sys/list.h>

typedef struct window
{
    list_entry_t base;
    point_t pos;
    surface_t surface;
    dwm_type_t type;
    bool invalid;
    bool moved;
    bool shown;
    rect_t prevRect;
    void (*cleanup)(struct window*);
    lock_t lock;
    msg_queue_t messages;
} window_t;

#define WINDOW_RECT(window) RECT_INIT_DIM(window->pos.x, window->pos.y, window->surface.width, window->surface.height);

#define WINDOW_INVALID_RECT(window) \
    RECT_INIT_DIM(window->pos.x + window->surface.invalidArea.left, window->pos.y + window->surface.invalidArea.top, \
        RECT_WIDTH(&window->surface.invalidArea), RECT_HEIGHT(&window->surface.invalidArea));

window_t* window_new(const point_t* pos, uint32_t width, uint32_t height, dwm_type_t type, void (*cleanup)(window_t*));

void window_free(window_t* window);

void window_populate_file(window_t* window, file_t* file);
