#pragma once

#include <sys/io.h>
#include <sys/list.h>
#include <win/dwm.h>

#include "gfx.h"

typedef struct window
{
    list_entry_t entry;
    point_t pos;
    gfx_t gfx;
    dwm_type_t type;
    bool invalid;
    bool moved;
    bool hidden;
    rect_t prevRect;
    //msg_queue_t messages;
} window_t;
