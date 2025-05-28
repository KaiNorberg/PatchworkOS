#include "dwm.h"

#include "client.h"
#include "compositor.h"
#include "kbd.h"
#include "screen.h"
#include "surface.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fb.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>
#include <threads.h>

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
static surface_t* fullscreen;
static surface_t* prevCursorTarget;

static surface_t* focus;

static poll_ctx_t* pollCtx;

static client_t* dwm_client_accept(void)
{
    printf("dwm: accept\n");
    fd_t fd = openf("sys:/net/local/%s/accept", id);
    if (fd == ERR)
    {
        printf("dwm: failed to open accept (%s)\n", strerror(errno));
        return NULL;
    }

    client_t* client = client_new(fd);
    if (client == NULL)
    {
        printf("dwm: failed to accept\n");
        close(fd);
        return NULL;
    }

    list_push(&clients, &client->entry);
    clientAmount++;
    return client;
}

static void dwm_client_disconnect(client_t* client)
{
    printf("dwm: disconnect\n");
    list_remove(&client->entry);
    client_free(client);
    clientAmount--;
}

static void dwm_send_event_to_all(surface_id_t target, event_type_t type, void* data, uint64_t size)
{
    client_t* client;
    client_t* temp;
    LIST_FOR_EACH_SAFE(client, temp, &clients, entry)
    {
        if (client_send_event(client, target, type, data, size) == ERR)
        {
            dwm_client_disconnect(client);
        }
    }
}

void dwm_init(void)
{
    fd_t handle = open("sys:/net/local/new?nonblock");
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
    fullscreen = NULL;
    prevCursorTarget = NULL;

    focus = NULL;

    pollCtx = NULL;
}

void dwm_deinit(void)
{
    close(kbd);
    close(mouse);
    close(handle);
    close(data);

    free(pollCtx);
}

void dwm_report_produce(surface_t* surface, client_t* client, report_flags_t flags)
{
    event_report_t event;
    event.flags = flags;
    surface_get_info(surface, &event.info);

    client_send_event(client, surface->id, EVENT_REPORT, &event, sizeof(event));

    event_global_report_t globalEvent;
    globalEvent.flags = flags;
    globalEvent.info = event.info;

    dwm_send_event_to_all(SURFACE_ID_NONE, EVENT_GLOBAL_REPORT, &globalEvent, sizeof(globalEvent));
}

surface_t* dwm_surface_find(surface_id_t id)
{
    surface_t* panel;
    LIST_FOR_EACH_REVERSE(panel, &panels, dwmEntry)
    {
        if (panel->id == id)
        {
            return panel;
        }
    }

    surface_t* window;
    LIST_FOR_EACH_REVERSE(window, &windows, dwmEntry)
    {
        if (window->id == id)
        {
            return window;
        }
    }

    if (wall != NULL && wall->id == id)
    {
        return wall;
    }

    if (fullscreen != NULL && fullscreen->id == id)
    {
        return fullscreen;
    }

    return NULL;
}

uint64_t dwm_attach(surface_t* surface)
{
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
            printf("dwm error: attach (cursor != NULL)\n");
            return ERR;
        }

        cursor = surface;
    }
    break;
    case SURFACE_WALL:
    {
        if (wall != NULL)
        {
            printf("dwm error: attach (wall != NULL)\n");
            return ERR;
        }

        wall = surface;
    }
    break;
    case SURFACE_FULLSCREEN:
    {
        if (fullscreen != NULL)
        {
            printf("dwm error: attach (fullscreen != NULL)\n");
            return ERR;
        }

        fullscreen = surface;
        focus = surface;
    }
    break;
    default:
    {
        printf("dwm error: attach (default)\n");
        return ERR;
    }
    }

    event_global_attach_t event;
    surface_get_info(surface, &event.info);
    dwm_send_event_to_all(SURFACE_ID_NONE, EVENT_GLOBAL_ATTACH, &event, sizeof(event));
    return 0;
}

void dwm_detach(surface_t* surface)
{
    if (surface == focus)
    {
        focus = NULL;
    }
    if (surface == prevCursorTarget)
    {
        prevCursorTarget = NULL;
    }

    event_global_detach_t event;
    surface_get_info(surface, &event.info);
    dwm_send_event_to_all(SURFACE_ID_NONE, EVENT_GLOBAL_DETACH, &event, sizeof(event));

    switch (surface->type)
    {
    case SURFACE_WINDOW:
    case SURFACE_PANEL:
    {
        list_remove(&surface->dwmEntry);
    }
    break;
    case SURFACE_CURSOR:
    {
        cursor = NULL;
    }
    break;
    case SURFACE_WALL:
    {
        wall = NULL;
    }
    break;
    case SURFACE_FULLSCREEN:
    {
        fullscreen = NULL;
        focus = NULL;
    }
    break;
    default:
    {
        printf("dwm error: attempt to detach invalid surface\n");
        exit(EXIT_FAILURE);
    }
    }

    if (wall != NULL)
    {
        compositor_set_total_redraw_needed();
    }
}

void dwm_focus_set(surface_t* surface)
{
    if (fullscreen != NULL)
    {
        return;
    }

    if (surface == focus)
    {
        return;
    }

    if (focus != NULL)
    {
        focus->isFocused = false;
        dwm_report_produce(focus, focus->client, REPORT_FOCUSED);
    }

    if (surface != NULL)
    {
        surface->isFocused = true;
        if (surface->type == SURFACE_WINDOW)
        {
            // Move to end of list
            list_remove(&surface->dwmEntry);
            list_push(&windows, &surface->dwmEntry);
            surface->hasMoved = true;
        }
        focus = surface;
        dwm_report_produce(focus, focus->client, REPORT_FOCUSED);
    }
    else
    {
        focus = NULL;
    }
}

static surface_t* dwm_surface_under_point(const point_t* point)
{
    if (fullscreen != NULL)
    {
        return fullscreen;
    }

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

// TODO: Cache this or something
static surface_t* dwm_next_timer(void)
{
    clock_t deadline = CLOCKS_NEVER;
    surface_t* nextTimer = NULL;

    surface_t* window;
    LIST_FOR_EACH(window, &windows, dwmEntry)
    {
        if (window->timer.deadline < deadline)
        {
            deadline = window->timer.deadline;
            nextTimer = window;
        }
    }

    surface_t* panel;
    LIST_FOR_EACH(panel, &panels, dwmEntry)
    {
        if (panel->timer.deadline < deadline)
        {
            deadline = panel->timer.deadline;
            nextTimer = panel;
        }
    }

    if (wall != NULL && wall->timer.deadline < deadline)
    {
        deadline = wall->timer.deadline;
        nextTimer = wall;
    }

    if (cursor != NULL && cursor->timer.deadline < deadline)
    {
        deadline = cursor->timer.deadline;
        nextTimer = cursor;
    }

    if (fullscreen != NULL && fullscreen->timer.deadline < deadline)
    {
        deadline = fullscreen->timer.deadline;
        nextTimer = fullscreen;
    }

    return nextTimer;
}

static void dwm_kbd_read(void)
{
    if (poll1(kbd, POLL_READ, 0) == POLL_READ)
    {
        // The kbd_event_t and event_kbd_t naming is a bit weird.
        kbd_event_t kbdEvent;
        if (read(kbd, &kbdEvent, sizeof(kbd_event_t)) != sizeof(kbd_event_t))
        {
            printf("dwm error: failed to read kbd event\n");
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
        client_send_event(focus->client, focus->id, EVENT_KBD, &event, sizeof(event_kbd_t));

        event_global_kbd_t globalEvent = event;
        dwm_send_event_to_all(SURFACE_ID_NONE, EVENT_GLOBAL_KBD, &globalEvent, sizeof(globalEvent));
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
    mouse_buttons_t pressed = mouseEvent->buttons & ~prevHeld;
    mouse_buttons_t released = prevHeld & ~mouseEvent->buttons;

    point_t oldCursorPos = cursor->pos;
    cursor->pos.x = CLAMP(cursor->pos.x + mouseEvent->deltaX, 0, (int64_t)screen_width() - 1);
    cursor->pos.y = CLAMP(cursor->pos.y + mouseEvent->deltaY, 0, (int64_t)screen_height() - 1);

    point_t cursorDelta = {.x = cursor->pos.x - oldCursorPos.x, .y = cursor->pos.y - oldCursorPos.y};
    if (fullscreen == NULL && (cursorDelta.x != 0 || cursorDelta.y != 0))
    {
        compositor_ctx_t ctx = {
            .windows = &windows,
            .panels = &panels,
            .wall = wall,
            .cursor = cursor,
        };
        compositor_redraw_cursor(&ctx);
    }

    surface_t* surface = dwm_surface_under_point(&cursor->pos);

    if (surface != prevCursorTarget)
    {
        if (prevCursorTarget != NULL)
        {
            event_cursor_leave_t event = {
                .held = held,
                .pressed = MOUSE_NONE,
                .released = MOUSE_NONE,
                .pos.x = cursor->pos.x - prevCursorTarget->pos.x,
                .pos.y = cursor->pos.y - prevCursorTarget->pos.y,
                .screenPos = cursor->pos,
                .delta = cursorDelta,
            };
            client_send_event(prevCursorTarget->client, prevCursorTarget->id, EVENT_CURSOR_LEAVE, &event,
                sizeof(event_cursor_leave_t));
        }

        if (surface != NULL)
        {
            event_cursor_enter_t event = {
                .held = held,
                .pressed = MOUSE_NONE,
                .released = MOUSE_NONE,
                .pos.x = cursor->pos.x - surface->pos.x,
                .pos.y = cursor->pos.y - surface->pos.y,
                .screenPos = cursor->pos,
                .delta = cursorDelta,
            };
            client_send_event(surface->client, surface->id, EVENT_CURSOR_ENTER, &event, sizeof(event_cursor_enter_t));
        }
        prevCursorTarget = surface;
    }

    if (pressed != MOUSE_NONE)
    {
        dwm_focus_set(surface);
    }

    surface_t* destSurface;
    if (held != MOUSE_NONE && focus != NULL)
    {
        destSurface = focus;
    }
    else
    {
        destSurface = surface;
    }

    if (destSurface != NULL)
    {
        event_mouse_t event = {
            .held = held,
            .pressed = pressed,
            .released = released,
            .pos.x = cursor->pos.x - destSurface->pos.x,
            .pos.y = cursor->pos.y - destSurface->pos.y,
            .screenPos = cursor->pos,
            .delta = cursorDelta,
        };
        client_send_event(destSurface->client, destSurface->id, EVENT_MOUSE, &event, sizeof(event_mouse_t));

        event_global_mouse_t globalEvent = event;
        globalEvent.pos = globalEvent.screenPos;
        dwm_send_event_to_all(SURFACE_ID_NONE, EVENT_GLOBAL_MOUSE, &globalEvent, sizeof(globalEvent));
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
            printf("dwm error: failed to read mouse event\n");
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

static void dwm_poll_ctx_update(void)
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

static void dwm_poll(void)
{
    dwm_poll_ctx_update();

    surface_t* timer = dwm_next_timer();
    clock_t timeout = CLOCKS_NEVER;
    if (timer != NULL)
    {
        clock_t time = uptime();
        timeout = timer->timer.deadline > time ? timer->timer.deadline - time : 0;
    }

    uint64_t events = poll((pollfd_t*)pollCtx, sizeof(poll_ctx_t) / sizeof(pollfd_t) + clientAmount, timeout);

    clock_t time = uptime();
    if (timer != NULL && time >= timer->timer.deadline)
    {
        if (timer->timer.flags & TIMER_REPEAT)
        {
            timer->timer.deadline = time + timer->timer.timeout;
        }
        else
        {
            timer->timer.deadline = CLOCKS_NEVER;
        }
        client_send_event(timer->client, timer->id, EVENT_TIMER, NULL, 0);
    }
}

static void dwm_update(void)
{
    dwm_poll();

    if (pollCtx->data.occurred & POLL_READ)
    {
        dwm_client_accept();
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
            if (client_receive_cmds(client) == ERR)
            {
                printf("dwm: client_receive_cmds failed (%s)\n", strerror(errno));
                dwm_client_disconnect(client);
            }
        }
    }

    compositor_ctx_t ctx = {
        .windows = &windows,
        .panels = &panels,
        .wall = wall,
        .cursor = cursor,
        .fullscreen = fullscreen,
    };
    compositor_draw(&ctx);
}

void dwm_loop(void)
{
    while (1)
    {
        dwm_update();
    }
}
