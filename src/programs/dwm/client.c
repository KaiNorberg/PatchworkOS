#include "client.h"

#include "compositor.h"
#include "dwm.h"
#include "screen.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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
    surface_t* wall = NULL;

    surface_t* surface;
    surface_t* temp;
    LIST_FOR_EACH_SAFE(surface, temp, &client->surfaces, clientEntry)
    {
        list_remove(&surface->clientEntry);
        dwm_detach(surface);
        surface_free(surface);
    }

    close(client->fd);
    free(client);
}

static uint64_t client_action_screen_info(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_screen_info_t))
    {
        return ERR;
    }
    cmd_screen_info_t* cmd = (cmd_screen_info_t*)header;

    if (cmd->index != 0)
    {
        return ERR;
    }

    event_screen_info_t event;
    if (cmd->index != 0)
    {
        event.width = 0;
        event.height = 0;
    }
    else
    {
        event.width = screen_width();
        event.height = screen_height();
    }
    client_send_event(client, SURFACE_ID_NONE, EVENT_SCREEN_INFO, &event, sizeof(event_screen_info_t));
    return 0;
}

static uint64_t client_action_surface_new(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_new_t))
    {
        return ERR;
    }
    cmd_surface_new_t* cmd = (cmd_surface_new_t*)header;

    if (cmd->type < 0 || cmd->type >= SURFACE_TYPE_AMOUNT)
    {
        return ERR;
    }
    if (RECT_WIDTH(&cmd->rect) <= 0 || RECT_HEIGHT(&cmd->rect) <= 0)
    {
        return ERR;
    }
    if (client_find_surface(client, cmd->id) != NULL)
    {
        return ERR;
    }

    const rect_t* rect = &cmd->rect;
    point_t point = {.x = rect->left, .y = rect->top};
    surface_t* surface = surface_new(client, cmd->id, &point, RECT_WIDTH(rect), RECT_HEIGHT(rect), cmd->type);
    if (surface == NULL)
    {
        return ERR;
    }

    if (dwm_attach(surface) == ERR)
    {
        surface_free(surface);
    }

    list_push(&client->surfaces, &surface->clientEntry);

    event_surface_new_t event;
    strcpy(event.shmem, surface->shmem);
    client_send_event(client, surface->id, EVENT_SURFACE_NEW, &event, sizeof(event));

    dwm_focus_set(surface);
    return 0;
}

static uint64_t client_action_surface_free(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_free_t))
    {
        return ERR;
    }
    cmd_surface_free_t* cmd = (cmd_surface_free_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    list_remove(&surface->clientEntry);
    dwm_detach(surface);
    surface_free(surface);
    return 0;
}

static uint64_t client_action_surface_move(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_move_t))
    {
        return ERR;
    }
    cmd_surface_move_t* cmd = (cmd_surface_move_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    uint64_t width = RECT_WIDTH(&cmd->rect);
    uint64_t height = RECT_HEIGHT(&cmd->rect);

    if (surface->gfx.width != width || surface->gfx.height != height)
    {
        // TODO: Reimplement resizing
        return ERR;
        /*if (surface_resize_buffer(surface, width, height) == ERR)
        {
            return ERR;
        }*/
    }

    surface->pos = (point_t){.x = cmd->rect.left, .y = cmd->rect.top};
    surface->moved = true;
    compositor_set_redraw_needed();

    event_surface_move_t event;
    event.rect = cmd->rect;
    client_send_event(surface->client, surface->id, EVENT_SURFACE_MOVE, &event, sizeof(event_surface_move_t));
    return 0;
}

static uint64_t client_action_surface_timer_set(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_timer_set_t))
    {
        return ERR;
    }
    cmd_surface_timer_set_t* cmd = (cmd_surface_timer_set_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    surface->timer.flags = cmd->flags;
    surface->timer.timeout = cmd->timeout;
    surface->timer.deadline = cmd->timeout == CLOCKS_NEVER ? CLOCKS_NEVER : uptime() + cmd->timeout;
    return 0;
}

static uint64_t client_action_surface_invalidate(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_invalidate_t))
    {
        return ERR;
    }
    cmd_surface_invalidate_t* cmd = (cmd_surface_invalidate_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_CONTENT_RECT(surface);
    rect_t invalidRect = cmd->invalidRect;
    RECT_FIT(&invalidRect, &surfaceRect);
    gfx_invalidate(&surface->gfx, &invalidRect);
    surface->invalid = true;
    compositor_set_redraw_needed();
    return 0;
}

static uint64_t client_action_surface_focus_set(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_focus_set_t))
    {
        return ERR;
    }
    cmd_surface_focus_set_t* cmd = (cmd_surface_focus_set_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    dwm_focus_set(surface);
    return 0;
}

static uint64_t (*actions[])(client_t*, const cmd_header_t*) = {
    [CMD_SCREEN_INFO] = client_action_screen_info,
    [CMD_SURFACE_NEW] = client_action_surface_new,
    [CMD_SURFACE_FREE] = client_action_surface_free,
    [CMD_SURFACE_MOVE] = client_action_surface_move,
    [CMD_SURFACE_TIMER_SET] = client_action_surface_timer_set,
    [CMD_SURFACE_INVALIDATE] = client_action_surface_invalidate,
    [CMD_SURFACE_FOCUS_SET] = client_action_surface_focus_set,
};

uint64_t client_receive_cmds(client_t* client)
{
    errno = 0;
    uint64_t readSize = read(client->fd, &client->cmds, sizeof(cmd_buffer_t) + 1);
    if (readSize == ERR)
    {
        if (errno != EWOULDBLOCK)
        {
            return ERR;
        }
        return 0;
    }

    if (readSize == 0)
    {
        printf("dwm client: end of file\n");
        return ERR;
    }

    if (readSize > sizeof(cmd_buffer_t)) // Program wrote to much or end of file
    {
        printf("dwm client: wrote to much to socket\n");
        return ERR;
    }

    if (readSize != client->cmds.size || client->cmds.size > CMD_BUFFER_MAX_DATA)
    {
        printf("dwm client: invalid cmd buffer size\n");
        return ERR;
    }

    uint64_t amount = 0;
    cmd_header_t* cmd;
    CMD_BUFFER_FOR_EACH(&client->cmds, cmd)
    {
        amount++;
        if (amount > client->cmds.amount || ((uint64_t)cmd + cmd->size - (uint64_t)&client->cmds) > readSize ||
            cmd->magic != CMD_MAGIC || cmd->type >= CMD_TYPE_AMOUNT)
        {
            printf("dwm client: corrupt cmd\n");
            return ERR;
        }
    }
    if (amount != client->cmds.amount)
    {
        printf("dwm client: invalid cmd amount\n");
        return ERR;
    }

    CMD_BUFFER_FOR_EACH(&client->cmds, cmd)
    {
        if (actions[cmd->type](client, cmd) == ERR)
        {
            printf("dwm client: cmd caused error\n");
            return ERR;
        }
    }

    return 0;
}

uint64_t client_send_event(client_t* client, surface_id_t target, event_type_t type, void* data, uint64_t size)
{
    event_t event = {.type = type, .target = target};
    memcpy(&event.raw, data, size);
    if (write(client->fd, &event, sizeof(event_t)) == ERR)
    {
        return ERR;
    }
    return 0;
}
