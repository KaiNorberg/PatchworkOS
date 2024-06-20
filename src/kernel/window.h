#pragma once

#include "defs.h"
#include "list.h"
#include "lock.h"
#include "message.h"
#include "vfs.h"

#include <sys/win.h>

typedef struct window
{
    list_entry_t base;
    point_t pos;
    surface_t surface;
    win_type_t type;
    bool invalid;
    lock_t lock;
    message_queue_t messages;
} window_t;

window_t* window_new(const point_t* pos, uint32_t width, uint32_t height, win_type_t type, file_t* file,
    void (*cleanup)(file_t* file));

void window_free(window_t* window);
