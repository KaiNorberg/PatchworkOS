#pragma once

#include "surface.h"

#include <sys/io.h>
#include <sys/list.h>
#include <libdwm/cmd.h>
#include <libdwm/event.h>

typedef struct client
{
    list_entry_t entry;
    fd_t fd;
    list_t surfaces;
    cmd_buffer_t cmds;
} client_t;

client_t* client_new(fd_t fd);

void client_free(client_t* client);

uint64_t client_recieve_cmds(client_t* client);

uint64_t client_send_event(client_t* client, event_type_t type, surface_id_t target, void* data, uint64_t size);
