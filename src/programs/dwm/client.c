#include "client.h"
#include "win/dwm.h"

#include <stdlib.h>
#include <stdio.h>

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

static uint64_t client_action_screen_info(const cmd_t* cmd)
{
    printf("screen info");
    return 0;
}

static uint64_t(*actions[])(const cmd_t*) = {
    [CMD_SCREEN_INFO] = client_action_screen_info,
};

uint64_t client_recieve_cmds(client_t* client)
{
    if (read(client->fd, &client->cmds, sizeof(cmd_buffer_t) + 1) > sizeof(cmd_buffer_t))
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

        if (actions[cmd->type](cmd) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

uint64_t client_send_event(client_t* client, const event_t* event)
{
    if (write(client->fd, event, sizeof(event_t)) == ERR)
    {

    }
}
