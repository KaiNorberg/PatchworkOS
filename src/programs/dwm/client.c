#include "client.h"
#include "screen.h"
#include "win/dwm.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

client_t* client_new(fd_t fd)
{
    client_t* client = malloc(sizeof(client_t));
    if (client == NULL)
    {
        return NULL;
    }
    list_entry_init(&client->entry);
    client->fd = fd;
    client->window = NULL;
    return client;
}

void client_free(client_t* client)
{
    close(client->fd);
    free(client);
}

static uint64_t client_action_screen_info(client_t* client, const cmd_t* cmd)
{
    event_screen_info_t screenInfo;
    if (cmd->screenInfo.index != 0)
    {
        screenInfo.width = 0;
        screenInfo.height = 0;
    }
    else
    {
        screenInfo.width = screen_width();
        screenInfo.height = screen_height();
    }

    client_send_event(client, EVENT_SCREEN_INFO, &screenInfo, sizeof(event_screen_info_t));
    return 0;
}

static uint64_t(*actions[])(client_t*, const cmd_t*) = {
    [CMD_SCREEN_INFO] = client_action_screen_info,
};

uint64_t client_recieve_cmds(client_t* client)
{
    uint64_t result = read(client->fd, &client->cmds, sizeof(cmd_buffer_t) + 1);
    if (result > sizeof(cmd_buffer_t) || result == 0) // Program wrote to much or end of file
    {
        return ERR;
    }

    if (client->cmds.amount > CMD_BUFFER_MAX_CMD)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < client->cmds.amount; i++)
    {
        cmd_t* cmd = &client->cmds.buffer[i];
        if (cmd->type >= CMD_TOTAL_AMOUNT)
        {
            return ERR;
        }

        if (actions[cmd->type](client, cmd) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

uint64_t client_send_event(client_t* client, event_type_t type, void* data, uint64_t size)
{
    event_t event = {.type = type};
    memcpy(&event.data, data, size);
    if (write(client->fd, &event, sizeof(event_t)) == ERR)
    {
        return ERR;
    }
    return 0;
}
