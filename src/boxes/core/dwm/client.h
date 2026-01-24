#pragma once

#include "surface.h"

#include <patchwork/cmd.h>
#include <patchwork/event.h>
#include <sys/fs.h>
#include <sys/list.h>

#define CLIENT_RECV_BUFFER_SIZE (sizeof(cmd_buffer_t) + 128)

typedef struct client
{
    list_entry_t entry;
    fd_t fd;
    list_t surfaces;
    event_bitmask_t bitmask;
    char recvBuffer[CLIENT_RECV_BUFFER_SIZE];
    size_t recvLen;
} client_t;

client_t* client_new(fd_t fd);

void client_free(client_t* client);

uint64_t client_receive_cmds(client_t* client);

uint64_t client_send_event(client_t* client, surface_id_t target, event_type_t type, void* data, uint64_t size);
