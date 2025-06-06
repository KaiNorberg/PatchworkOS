#pragma once

#include "surface.h"

#include <libpatchwork/event.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct client
{
    list_entry_t entry;
    fd_t fd;
    list_t surfaces;
    cmd_buffer_t cmds;
    uint64_t newId;
    event_bitmask_t bitmask;
} client_t;

client_t* client_new(fd_t fd);

void client_free(client_t* client);

uint64_t client_receive_cmds(client_t* client);

uint64_t client_send_event(client_t* client, surface_id_t target, event_type_t type, void* data, uint64_t size);
