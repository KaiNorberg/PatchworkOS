#pragma once

#include "window.h"

#include <sys/io.h>
#include <sys/list.h>

typedef struct
{
    list_entry_t entry;
    fd_t fd;
    window_t* window;
    cmd_buffer_t cmds;
    uint64_t padding;
} client_t;

client_t* client_new(fd_t fd);

void client_free(client_t* client);

uint64_t client_recieve_cmds(client_t* client);

uint64_t client_send_event(client_t* client, const event_t* event);
