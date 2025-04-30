#include "dwm.h"

#include "compositor.h"
#include "screen.h"
#include "surface.h"
#include "kbd.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <sys/fb.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>
#include <sys/math.h>

static fd_t handle;
static char id[MAX_NAME];
static fd_t data;

static fd_t kbd;
static fd_t mouse;

static list_t clients;
static uint64_t clientAmount;

static list_t windows;
static list_t panels;
static surface_t* wall;
static surface_t* cursor;

static surface_t* focus;

static poll_ctx_t* pollCtx;

static void dwm_focus_set(surface_t* surface);
static void dwm_surface_free(surface_t* surface);

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
    client_send_event(client, EVENT_SCREEN_INFO, SURFACE_ID_NONE, &event, sizeof(event_screen_info_t));
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

    dwm_surface_free(surface);
    return 0;
}

static uint64_t client_action_draw_rect(client_t* client, const cmd_header_t* header)
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

static uint64_t client_action_draw_edge(client_t* client, const cmd_header_t* header)
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

static uint64_t client_action_draw_gradient(client_t* client, const cmd_header_t* header)
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
    [CMD_SCREEN_INFO] = client_action_screen_info,
    [CMD_SURFACE_NEW] = client_action_surface_new,
    [CMD_SURFACE_FREE] = client_action_surface_free,
    [CMD_DRAW_RECT] = client_action_draw_rect,
    [CMD_DRAW_EDGE] = client_action_draw_edge,
    [CMD_DRAW_GRADIENT] = client_action_draw_gradient,
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

static void dwm_focus_set(surface_t* surface)
{
    if (surface == focus)
    {
        return;
    }

    if (focus != NULL)
    {
        client_send_event(focus->client, EVENT_FOCUS_OUT, focus->id, NULL, 0);
    }

    if (surface != NULL)
    {
        client_send_event(surface->client, EVENT_FOCUS_IN, surface->id, NULL, 0);
        focus = surface;
    }
    else
    {
        focus = NULL;
    }
}

static void dwm_surface_free(surface_t* surface)
{
    if (surface == focus)
    {
        focus = NULL;
    }

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

void dwm_init(void)
{
    fd_t handle = open("sys:/net/local/new");
    read(handle, id, MAX_NAME);

    fd_t ctl = openf("sys:/net/local/%s/ctl", id);
    writef(ctl, "bind dwm");
    writef(ctl, "listen");
    close(ctl);

    data = openf("sys:/net/local/%s/data", id);

    // TODO: Add system for choosing device
    kbd = open("sys:/kbd/ps2");
    mouse = open("sys:/mouse/ps2");

    list_init(&clients);
    clientAmount = 0;

    list_init(&windows);
    list_init(&panels);
    wall = NULL;
    cursor = NULL;

    focus = NULL;

    pollCtx = malloc(sizeof(poll_ctx_t));
}

void dwm_deinit(void)
{
    close(kbd);
    close(mouse);
    close(handle);
    close(data);

    free(pollCtx);
}

static surface_t* dwm_surface_under_point(const point_t* point)
{
    surface_t* panel;
    LIST_FOR_EACH_REVERSE(panel, &panels, dwmEntry)
    {
        rect_t rect = SURFACE_RECT(panel);
        if (RECT_CONTAINS_POINT(&rect, point))
        {
            return panel;
        }
    }

    surface_t* window;
    LIST_FOR_EACH_REVERSE(window, &windows, dwmEntry)
    {
        rect_t rect = SURFACE_RECT(window);
        if (RECT_CONTAINS_POINT(&rect, point))
        {
            return window;
        }
    }

    if (wall == NULL)
    {
        return NULL;
    }

    rect_t wallRect = SURFACE_RECT(wall);
    if (RECT_CONTAINS_POINT(&wallRect, point))
    {
        return wall;
    }

    return NULL;
}

static void dwm_poll_ctx_update()
{
    pollCtx = realloc(pollCtx, sizeof(poll_ctx_t) + (sizeof(pollfd_t) * clientAmount));
    pollCtx->data.fd = data;
    pollCtx->data.requested = POLL_READ;
    pollCtx->data.occurred = 0;
    pollCtx->kbd.fd = kbd;
    pollCtx->kbd.requested = POLL_READ;
    pollCtx->kbd.occurred = 0;
    pollCtx->mouse.fd = mouse;
    pollCtx->mouse.requested = POLL_READ;
    pollCtx->mouse.occurred = 0;

    uint64_t i = 0;
    client_t* client;
    LIST_FOR_EACH(client, &clients, entry)
    {
        pollfd_t* fd = &pollCtx->clients[i++];
        fd->fd = client->fd;
        fd->requested = POLL_READ;
        fd->occurred = 0;
    }
}

static void dwm_kbd_read(void)
{
    if (poll1(kbd, POLL_READ, 0) == POLL_READ)
    {
        // The kbd_event_t and event_kbd_t naming is a bit weird.
        kbd_event_t kbdEvent;
        if (read(kbd, &kbdEvent, sizeof(kbd_event_t)) != sizeof(kbd_event_t))
        {
            printf("dwm error: failed to read kbd event");
            return;
        }

        if (focus == NULL)
        {
            return;
        }

        event_kbd_t event;
        event.type = kbdEvent.type;
        event.mods = kbdEvent.mods;
        event.code = kbdEvent.code;
        event.ascii = kbd_ascii(event.code, event.mods);
        client_send_event(focus->client, EVENT_KBD, focus->id, &event, sizeof(event_kbd_t));
    }
}

static void dwm_handle_mouse_event(const mouse_event_t* mouseEvent)
{
    static mouse_buttons_t prevHeld = MOUSE_NONE;

    if (cursor == NULL)
    {
        return;
    }

    mouse_buttons_t held = mouseEvent->buttons;
    mouse_buttons_t pressed =  mouseEvent->buttons & ~prevHeld;
    mouse_buttons_t released = prevHeld & ~ mouseEvent->buttons;

    point_t oldCursorPos = cursor->pos;
    cursor->pos.x = CLAMP(cursor->pos.x + mouseEvent->deltaX, 0, (int64_t)screen_width() - 1);
    cursor->pos.y = CLAMP(cursor->pos.y + mouseEvent->deltaY, 0, (int64_t)screen_height() - 1);

    point_t cursorDelta = {.x = cursor->pos.x - oldCursorPos.x, .y = cursor->pos.y - oldCursorPos.y};
    if (cursorDelta.x != 0 || cursorDelta.y != 0)
    {
        compositor_ctx_t ctx = {
            .windows = &windows,
            .panels = &panels,
            .wall = wall,
            .cursor = cursor,
        };
        compositor_redraw_cursor(&ctx);
    }

    if (pressed != MOUSE_NONE && prevHeld == MOUSE_NONE)
    {
        surface_t* surface = dwm_surface_under_point(&cursor->pos);
        dwm_focus_set(surface);

        if (surface != NULL)
        {
            event_mouse_t event = {
                .held = held,
                .pressed = pressed,
                .released = released,
                .pos = cursor->pos,
                .delta = cursorDelta,
            };
            client_send_event(surface->client, EVENT_MOUSE, surface->id, &event, sizeof(event_mouse_t));
        }
    }

    prevHeld = held;
}

static void dwm_mouse_read(void)
{
    static mouse_buttons_t prevHeld = MOUSE_NONE;

    mouse_event_t total = {0};
    bool received = false;
    while (1)
    {
        if (poll1(mouse, POLL_READ, 0) != POLL_READ)
        {
            break;
        }

        mouse_event_t mouseEvent;
        if (read(mouse, &mouseEvent, sizeof(mouse_event_t)) != sizeof(mouse_event_t))
        {
            printf("dwm error: failed to read mouse event");
            return;
        }

        total.buttons |= mouseEvent.buttons;
        total.deltaX += mouseEvent.deltaX;
        total.deltaY += mouseEvent.deltaY;
        received = true;
    }

    if (!received)
    {
        return;
    }

    dwm_handle_mouse_event(&total);
}

static void dwm_poll(void)
{
    dwm_poll_ctx_update();
    while (poll((pollfd_t*)pollCtx, sizeof(poll_ctx_t) / sizeof(pollfd_t) + clientAmount, SEC / 60) == 0 && !compositor_redraw_needed())
        ;

    if (pollCtx->data.occurred & POLL_READ)
    {
        client_accept();
    }
    if (pollCtx->kbd.occurred & POLL_READ)
    {
        dwm_kbd_read();
    }
    if (pollCtx->mouse.occurred & POLL_READ)
    {
        dwm_mouse_read();
    }

    uint64_t i = 0;
    client_t* client;
    client_t* temp;
    LIST_FOR_EACH_SAFE(client, temp, &clients, entry)
    {
        pollfd_t* fd = &pollCtx->clients[i++];
        if (fd->occurred & POLL_READ)
        {
            if (client_recieve_cmds(client) == ERR)
            {
                client_free(client);
            }
        }
    }

    compositor_ctx_t ctx = {
        .windows = &windows,
        .panels = &panels,
        .wall = wall,
        .cursor = cursor,
    };
    compositor_draw(&ctx);
}

void dwm_loop(void)
{
    while (1)
    {
        dwm_poll();
    }
}
