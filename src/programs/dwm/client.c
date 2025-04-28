#include "client.h"
#include "screen.h"
#include "dwm.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static surface_t* client_find_surface(client_t* client, surface_id_t id)
{
    surface_t* surface;
    LIST_FOR_EACH(surface, &client->surfaces, clientEntry)
    {
        if (surface->id == id)
        {
            return surface;
        }
    }
    return NULL;
}

client_t* client_new(fd_t fd)
{
    client_t* client = malloc(sizeof(client_t));
    if (client == NULL)
    {
        return NULL;
    }
    list_entry_init(&client->entry);
    client->fd = fd;
    list_init(&client->surfaces);
    return client;
}

void client_free(client_t* client)
{
    printf("dwm: client free");
    /*while (1)
    {
        surface_t* surface = LIST_CONTAINER(list_pop(&client->surfaces), surface_t, clientEntry);
        if (surface == NULL)
        {
            break;
        }
        surface_free(surface);
    }*/

    close(client->fd);
    free(client);
}

static uint64_t client_action_screen_info(client_t* client, const cmd_t* cmd)
{
    if (cmd->screenInfo.index != 0)
    {
        return ERR;
    }

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

    client_send_event(client, EVENT_SCREEN_INFO, SURFACE_ID_ROOT, &screenInfo, sizeof(event_screen_info_t));
    return 0;
}

static uint64_t client_action_surface_new(client_t* client, const cmd_t* cmd)
{
    if (cmd->surfaceNew.type < 0 || cmd->surfaceNew.type >= SURFACE_TYPE_AMOUNT)
    {
        return ERR;
    }
    if (RECT_WIDTH(&cmd->surfaceNew.rect) <= 0 || RECT_HEIGHT(&cmd->surfaceNew.rect) <= 0)
    {
        return ERR;
    }
    if (client_find_surface(client, cmd->surfaceNew.id) != NULL)
    {
        return ERR;
    }

    const rect_t* rect = &cmd->surfaceNew.rect;
    point_t point = {.x = rect->left, .y = rect->top};
    surface_t* surface = surface_new(client, cmd->surfaceNew.id, &point, RECT_WIDTH(rect), RECT_HEIGHT(rect), cmd->surfaceNew.type);
    if (surface == NULL)
    {
        return ERR;
    }

    if (surface->type != SURFACE_WINDOW || cmd->surfaceNew.parent == SURFACE_ID_ROOT)
    {
        if (dwm_attach_to_wall(surface) == ERR)
        {
            surface_free(surface);
            return ERR;
        }
    }
    else if (cmd->surfaceNew.parent != SURFACE_ID_ROOT) // Is window
    {
        surface_t* parent = client_find_surface(client, cmd->surfaceNew.parent);
        if (parent == NULL)
        {
            surface_free(surface);
            return ERR;
        }
        list_push(&parent->children, &surface->surfaceEntry);
    }
    else
    {
        surface_free(surface);
        return ERR;
    }

    list_push(&client->surfaces, &surface->clientEntry);
    client_send_event(client, EVENT_INIT, surface->id, NULL, 0);
    client_send_event(client, EVENT_REDRAW, surface->id, NULL, 0);
    return 0;
}

static uint64_t client_action_draw_rect(client_t* client, const cmd_t* cmd)
{
    surface_t* surface = client_find_surface(client, cmd->drawRect.target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_RECT(surface);
    rect_t rect = cmd->drawRect.rect;
    RECT_FIT(&rect, &surfaceRect);
    gfx_rect(&surface->gfx, &rect, cmd->drawRect.pixel);

    surface->invalid = true;
    dwm_set_redraw_needed();
    return 0;
}

static uint64_t(*actions[])(client_t*, const cmd_t*) = {
    [CMD_SCREEN_INFO] = client_action_screen_info,
    [CMD_SURFACE_NEW] = client_action_surface_new,
    [CMD_DRAW_RECT] = client_action_draw_rect,
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

uint64_t client_send_event(client_t* client, event_type_t type, surface_id_t target, void* data, uint64_t size)
{
    event_t event = {.type = type, .target = target};
    memcpy(&event.data, data, size);
    if (write(client->fd, &event, sizeof(event_t)) == ERR)
    {
        return ERR;
    }
    return 0;
}
