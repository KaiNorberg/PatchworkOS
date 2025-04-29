#include "dwm.h"

#include "surface.h"
#include "screen.h"
#include "compositor.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <sys/fb.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>

static fd_t handle;
static char id[MAX_NAME];

// static fd_t kbd;
// static fd_t mouse;

static list_t clients;
static uint64_t clientAmount;

static list_t windows;
static list_t panels;
static surface_t* wall;
static surface_t* cursor;

static void dwm_surface_free(surface_t* surface)
{
    switch (surface->type)
    {
    case SURFACE_WINDOW:
    case SURFACE_PANEL:
    {
        list_remove(&surface->dwmEntry);
        list_remove(&surface->clientEntry);
        surface_free(surface);
    }
    break;
    case SURFACE_CURSOR:
    {
        cursor = NULL;
        list_remove(&surface->clientEntry);
        surface_free(surface);
    }
    break;
    case SURFACE_WALL:
    {
        wall = NULL;
        list_remove(&surface->clientEntry);
        surface_free(surface);
    }
    break;
    default:
    {
        printf("dwm error: attempt to free invalid surface");
        exit(EXIT_FAILURE);
    }
    }

    if (wall != NULL)
    {
        wall->moved = true;
        compositor_set_redraw_needed();
    }
}

static client_t* client_accept(void)
{
    printf("dwm: client accept");
    fd_t fd = openf("sys:/net/local/%s/accept", id);
    if (fd == ERR)
    {
        return NULL;
    }

    client_t* client = malloc(sizeof(client_t));
    if (client == NULL)
    {
        printf("dwm error: failed to accept client");
        close(fd);
        return NULL;
    }
    list_entry_init(&client->entry);
    client->fd = fd;
    list_init(&client->surfaces);

    list_push(&clients, &client->entry);
    clientAmount++;
    return client;
}

static void client_free(client_t* client)
{
    list_remove(&client->entry);
    printf("dwm client: free");

    surface_t* wall = NULL;

    surface_t* surface;
    surface_t* temp;
    LIST_FOR_EACH_SAFE(surface, temp, &client->surfaces, clientEntry)
    {
        dwm_surface_free(surface);
    }

    close(client->fd);
    free(client);
    clientAmount--;
}

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

static uint64_t client_send_event(client_t* client, event_type_t type, surface_id_t target, void* data, uint64_t size)
{
    event_t event = {.type = type, .target = target};
    memcpy(&event.data, data, size);
    if (write(client->fd, &event, sizeof(event_t)) == ERR)
    {
        return ERR;
    }
    return 0;
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

    client_send_event(client, EVENT_SCREEN_INFO, SURFACE_ID_NONE, &screenInfo, sizeof(event_screen_info_t));
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

    switch (surface->type)
    {
    case SURFACE_WINDOW:
    {
        list_push(&windows, &surface->dwmEntry);
    }
    break;
    case SURFACE_PANEL:
    {
        list_push(&panels, &surface->dwmEntry);
    }
    break;
    case SURFACE_CURSOR:
    {
        if (cursor != NULL)
        {
            printf("dwm error: attach (cursor != NULL)");
            surface_free(surface);
            return ERR;
        }

        cursor = surface;
    }
    break;
    case SURFACE_WALL:
    {
        if (wall != NULL)
        {
            printf("dwm error: attach (wall != NULL)");
            surface_free(surface);
            return ERR;
        }

        wall = surface;
    }
    break;
    default:
    {
        printf("dwm error: attach (default)");
        surface_free(surface);
        return ERR;
    }
    }

    list_push(&client->surfaces, &surface->clientEntry);

    client_send_event(client, EVENT_INIT, surface->id, NULL, 0);
    client_send_event(client, EVENT_REDRAW, surface->id, NULL, 0);
    return 0;
}

static uint64_t client_action_surface_free(client_t* client, const cmd_t* cmd)
{
    surface_t* surface = client_find_surface(client, cmd->drawRect.target);
    if (surface == NULL)
    {
        return ERR;
    }

    dwm_surface_free(surface);

    return 0;
}

static uint64_t client_action_draw_rect(client_t* client, const cmd_t* cmd)
{
    surface_t* surface = client_find_surface(client, cmd->drawRect.target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_LOCAL_RECT(surface);
    rect_t rect = cmd->drawRect.rect;
    RECT_FIT(&rect, &surfaceRect);
    gfx_rect(&surface->gfx, &rect, cmd->drawRect.pixel);

    surface->invalid = true;
    compositor_set_redraw_needed();
    return 0;
}

static uint64_t client_action_draw_edge(client_t* client, const cmd_t* cmd)
{
    surface_t* surface = client_find_surface(client, cmd->drawEdge.target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_LOCAL_RECT(surface);
    rect_t rect = cmd->drawEdge.rect;
    RECT_FIT(&rect, &surfaceRect);
    gfx_edge(&surface->gfx, &rect, cmd->drawEdge.width, cmd->drawEdge.foreground, cmd->drawEdge.background);

    surface->invalid = true;
    compositor_set_redraw_needed();
    return 0;
}

static uint64_t client_action_draw_gradient(client_t* client, const cmd_t* cmd)
{
    surface_t* surface = client_find_surface(client, cmd->drawGradient.target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_LOCAL_RECT(surface);
    rect_t rect = cmd->drawGradient.rect;
    RECT_FIT(&rect, &surfaceRect);
    gfx_gradient(&surface->gfx, &rect, cmd->drawGradient.start, cmd->drawGradient.end, cmd->drawGradient.type, cmd->drawGradient.addNoise);

    surface->invalid = true;
    compositor_set_redraw_needed();
    return 0;
}

static uint64_t(*actions[])(client_t*, const cmd_t*) = {
    [CMD_SCREEN_INFO] = client_action_screen_info,
    [CMD_SURFACE_NEW] = client_action_surface_new,
    [CMD_SURFACE_FREE] = client_action_surface_free,
    [CMD_DRAW_RECT] = client_action_draw_rect,
    [CMD_DRAW_EDGE] = client_action_draw_edge,
    [CMD_DRAW_GRADIENT] = client_action_draw_gradient,
};

static uint64_t client_recieve_cmds(client_t* client)
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

void dwm_init(void)
{
    fd_t handle = open("sys:/net/local/new");
    read(handle, id, MAX_NAME);

    fd_t ctl = openf("sys:/net/local/%s/ctl", id);
    writef(ctl, "bind dwm");
    writef(ctl, "listen");
    close(ctl);

    // kbd = open("sys:/kbd/ps2");
    // mouse = open("sys:/mouse/ps2");

    list_init(&clients);
    clientAmount = 0;

    list_init(&windows);
    list_init(&panels);
    wall = NULL;
    cursor = NULL;
}

void dwm_deinit(void)
{
    close(handle);
}

static void dwm_poll(fd_t data)
{
    pollfd_t* fds = malloc(sizeof(pollfd_t) * (1 + clientAmount));
    fds[0].fd = data;
    fds[0].requested = POLL_READ;
    fds[0].occurred = 0;

    uint64_t i = 1;
    client_t* client;
    LIST_FOR_EACH(client, &clients, entry)
    {
        pollfd_t* fd = &fds[i++];
        fd->fd = client->fd;
        fd->requested = POLL_READ;
        fd->occurred = 0;
    }
    while (poll(fds, 1 + clientAmount, SEC / 60) == 0 && !compositor_redraw_needed());

    if (fds[0].occurred & POLL_READ)
    {
        client_accept();
    }

    i = 1;
    client_t* temp;
    LIST_FOR_EACH_SAFE(client, temp, &clients, entry)
    {
        pollfd_t* fd = &fds[i++];
        if (fd->occurred & POLL_READ)
        {
            if (client_recieve_cmds(client) == ERR)
            {
                client_free(client);
            }
        }
    }

    free(fds);

    compositor_ctx_t ctx =
    {
        .windows = &windows,
        .panels = &panels,
        .wall = wall,
        .cursor = cursor,
    };
    compositor_redraw(&ctx);
}

void dwm_loop(void)
{
    fd_t data = openf("sys:/net/local/%s/data", id);

    while (1)
    {
        dwm_poll(data);
    }

    close(data);
}
