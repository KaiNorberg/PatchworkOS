#pragma once

#include "surface.h"

#include <sys/io.h>
#include <sys/list.h>
#include <libdwm/event.h>

typedef struct client
{
    list_entry_t entry;
    fd_t fd;
    list_t surfaces;
    cmd_buffer_t cmds;
} client_t;

void dwm_init(void);

void dwm_deinit(void);

void dwm_loop(void);
