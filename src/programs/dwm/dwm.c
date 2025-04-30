#include "dwm.h"

#include "compositor.h"
#include "screen.h"
#include "surface.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fb.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>
#include <threads.h>

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

static uint64_t dwm_action_screen_info(client_t* client, const cmd_header_t* header)
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
    client_send_event(client, EVENT_SCREEN_INFO, SURFACE_ID_NONE, &event, sizeof(event_screen_info_t));
    return 0;
}

static uint64_t dwm_action_surface_new(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_new_t))
    {
        printf("test1");
        return ERR;
    }
    cmd_surface_new_t* cmd = (cmd_surface_new_t*)header;

    if (cmd->type < 0 || cmd->type >= SURFACE_TYPE_AMOUNT)
    {
        printf("test2");
        return ERR;
    }
    if (RECT_WIDTH(&cmd->rect) <= 0 || RECT_HEIGHT(&cmd->rect) <= 0)
    {
        printf("test3 id: %d rect: %d %d %d %d", cmd->id, cmd->rect.left, cmd->rect.top, cmd->rect.right, cmd->rect.bottom);
        return ERR;
    }
    if (client_find_surface(client, cmd->id) != NULL)
    {
        printf("test4");
        return ERR;
    }

    const rect_t* rect = &cmd->rect;
    point_t point = {.x = rect->left, .y = rect->top};
    surface_t* surface = surface_new(client, cmd->id, &point, RECT_WIDTH(rect), RECT_HEIGHT(rect), cmd->type);
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

static uint64_t dwm_action_surface_free(client_t* client, const cmd_header_t* header)
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

    dwm_surface_free(surface);
    return 0;
}

static uint64_t dwm_action_draw_rect(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_draw_rect_t))
    {
        return ERR;
    }
    cmd_draw_rect_t* cmd = (cmd_draw_rect_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_CONTENT_RECT(surface);
    rect_t rect = cmd->rect;
    RECT_FIT(&rect, &surfaceRect);
    gfx_rect(&surface->gfx, &rect, cmd->pixel);

    surface->invalid = true;
    compositor_set_redraw_needed();
    return 0;
}

static uint64_t dwm_action_draw_edge(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_draw_edge_t))
    {
        return ERR;
    }
    cmd_draw_edge_t* cmd = (cmd_draw_edge_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_CONTENT_RECT(surface);
    rect_t rect = cmd->rect;
    RECT_FIT(&rect, &surfaceRect);
    gfx_edge(&surface->gfx, &rect, cmd->width, cmd->foreground, cmd->background);

    surface->invalid = true;
    compositor_set_redraw_needed();
    return 0;
}

static uint64_t dwm_action_draw_gradient(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_draw_gradient_t))
    {
        return ERR;
    }
    cmd_draw_gradient_t* cmd = (cmd_draw_gradient_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_CONTENT_RECT(surface);
    rect_t rect = cmd->rect;
    RECT_FIT(&rect, &surfaceRect);
    gfx_gradient(&surface->gfx, &rect, cmd->start, cmd->end, cmd->type, cmd->addNoise);

    surface->invalid = true;
    compositor_set_redraw_needed();
    return 0;
}

static uint64_t (*actions[])(client_t*, const cmd_header_t*) = {
    [CMD_SCREEN_INFO] = dwm_action_screen_info,
    [CMD_SURFACE_NEW] = dwm_action_surface_new,
    [CMD_SURFACE_FREE] = dwm_action_surface_free,
    [CMD_DRAW_RECT] = dwm_action_draw_rect,
    [CMD_DRAW_EDGE] = dwm_action_draw_edge,
    [CMD_DRAW_GRADIENT] = dwm_action_draw_gradient,
};

static uint64_t client_recieve_cmds(client_t* client)
{
    uint64_t readSize = read(client->fd, &client->cmds, sizeof(cmd_buffer_t) + 1); // Add plus one to check if packet is to big
    if (readSize > sizeof(cmd_buffer_t) || readSize == 0)                          // Program wrote to much or end of file
    {
        return ERR;
    }

    if (readSize != client->cmds.size || client->cmds.size > CMD_BUFFER_MAX_DATA)
    {
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
            return ERR;
        }
    }
    if (amount != client->cmds.amount)
    {
        return ERR;
    }

    CMD_BUFFER_FOR_EACH(&client->cmds, cmd)
    {
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
    while (poll(fds, 1 + clientAmount, SEC / 60) == 0 && !compositor_redraw_needed())
        ;

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

    compositor_ctx_t ctx = {
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
