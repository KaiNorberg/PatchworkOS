#pragma once

#include "psf.h"
#include "surface.h"

#include <libdwm/event.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct client
{
    list_entry_t entry;
    fd_t fd;
    list_t surfaces;
    list_t fonts;
    cmd_buffer_t cmds;
    uint64_t newId;
} client_t;

client_t* client_new(fd_t fd);

void client_free(client_t* client);

uint64_t client_receive_cmds(client_t* client);

uint64_t client_send_event(client_t* client, surface_id_t target, event_type_t type, void* data, uint64_t size);
