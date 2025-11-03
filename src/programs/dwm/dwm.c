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
    fd_t fd = openf("/net/local/%s/accept:nonblock", id);
    if (fd == ERR)
    {
        printf("dwm: failed to open accept file (%s)\n", strerror(errno));
        return NULL;
    }

    client_t* client = client_new(fd);
    if (client == NULL)
    {
        printf("dwm: failed to accept client (%s)\n", strerror(errno));
        close(fd);
        return NULL;
    }

    list_push(&clients, &client->entry);
    clientAmount++;
    printf("dwm: accepted client %d total %lu\n", client->fd, clientAmount);
    return client;
}

static void dwm_client_disconnect(client_t* client)
{
    list_remove(&clients, &client->entry);
    client_free(client);
    clientAmount--;
    printf("dwm: disconnect client\n");
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
    if (readfile("/net/local/seqpacket:nonblock", id, MAX_NAME - 1, 0) == ERR)
    {
        printf("dwm: failed to create socket (%s)\n", strerror(errno));
        abort();
    }

    fd_t ctl = openf("/net/local/%s/ctl", id);
    if (ctl == ERR)
    {
        printf("dwm: failed to open control file (%s)\n", strerror(errno));
        abort();
    }
    if (writef(ctl, "bind dwm") == ERR)
    {
        printf("dwm: failed to bind socket (%s)\n", strerror(errno));
        abort();
    }
    if (writef(ctl, "listen") == ERR)
    {
        printf("dwm: failed to listen (%s)\n", strerror(errno));
        abort();
    }
    close(ctl);

    data = openf("/net/local/%s/data", id);
    if (data == ERR)
    {
        printf("dwm: failed to open data file (%s)\n", strerror(errno));
        abort();
    }

    kbd = open("/dev/kbd/0/events");
    if (kbd == ERR)
    {
        printf("dwm: failed to open keyboard (%s)\n", strerror(errno));
        abort();
    }

    char name[MAX_NAME] = {0};
    if (readfile("/dev/kbd/0/name", name, MAX_NAME - 1, 0) != ERR)
    {
        printf("dwm: using keyboard '%s'\n", name);
    }

    mouse = open("/dev/mouse/0/events");
    if (mouse == ERR)
    {
        printf("dwm: failed to open mouse (%s)\n", strerror(errno));
        abort();
    }

    memset(name, 0, MAX_NAME);
    if (readfile("/dev/mouse/0/name", name, MAX_NAME - 1, 0) != ERR)
    {
        printf("dwm: using mouse '%s'\n", name);
    }

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
    close(data);

    free(pollCtx);
}

void dwm_report_produce(surface_t* surface, client_t* client, report_flags_t flags)
{
    event_report_t event;
    event.flags = flags;
    surface_get_info(surface, &event.info);

    client_send_event(client, surface->id, EVENT_REPORT, &event, sizeof(event));

    event_global_report_t globaEVENT_LIB;
    globaEVENT_LIB.flags = flags;
    globaEVENT_LIB.info = event.info;

    dwm_send_event_to_all(SURFACE_ID_NONE, EVENT_GLOBAL_REPORT, &globaEVENT_LIB, sizeof(globaEVENT_LIB));
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
            printf("dwm: attach (cursor != NULL)\n");
            errno = EALREADY;
            return ERR;
        }

        cursor = surface;
    }
    break;
    case SURFACE_WALL:
    {
        if (wall != NULL)
        {
            printf("dwm: attach (wall != NULL)\n");
            errno = EALREADY;
            return ERR;
        }

        wall = surface;
    }
    break;
    case SURFACE_FULLSCREEN:
    {
        if (fullscreen != NULL)
        {
            printf("dwm: attach (fullscreen != NULL)\n");
            errno = EALREADY;
            return ERR;
        }

        fullscreen = surface;
        focus = surface;
    }
    break;
    default:
    {
        printf("dwm: attach (default)\n");
        errno = EINVAL;
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
    {
        list_remove(&windows, &surface->dwmEntry);
    }
    break;
    case SURFACE_PANEL:
    {
        list_remove(&panels, &surface->dwmEntry);
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
        printf("dwm: attempt to detach invalid surface\n");
        abort();
    }
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
        focus->flags &= ~SURFACE_FOCUSED;
        dwm_report_produce(focus, focus->client, REPORT_IS_FOCUSED);
    }

    if (surface != NULL)
    {
        surface->flags |= SURFACE_FOCUSED;
        if (surface->type == SURFACE_WINDOW)
        {
            // Move to end of list
            list_remove(&windows, &surface->dwmEntry);
            list_push(&windows, &surface->dwmEntry);
        }
        focus = surface;
        dwm_report_produce(focus, focus->client, REPORT_IS_FOCUSED);
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
        rect_t rect = SURFACE_SCREEN_RECT(panel);
        if (RECT_CONTAINS_POINT(&rect, point))
        {
            return panel;
        }
    }

    surface_t* window;
    LIST_FOR_EACH_REVERSE(window, &windows, dwmEntry)
    {
        rect_t rect = SURFACE_SCREEN_RECT(window);
        if (RECT_CONTAINS_POINT(&rect, point))
        {
            return window;
        }
    }

    if (wall == NULL)
    {
        return NULL;
    }

    rect_t wallRect = SURFACE_SCREEN_RECT(wall);
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
    if (poll1(kbd, POLLIN, 0) == POLLIN)
    {
        // The kbd_event_t and event_kbd_t naming is a bit weird.
        kbd_event_t kbdEvent;
        if (read(kbd, &kbdEvent, sizeof(kbd_event_t)) != sizeof(kbd_event_t))
        {
            printf("dwm: failed to read kbd event\n");
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

        event_global_kbd_t globaEVENT_LIB = event;
        dwm_send_event_to_all(SURFACE_ID_NONE, EVENT_GLOBAL_KBD, &globaEVENT_LIB, sizeof(globaEVENT_LIB));
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
        rect_t surfaceRect = SURFACE_SCREEN_RECT(surface);
        compositor_invalidate(&surfaceRect);
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

        event_global_mouse_t globaEVENT_LIB = event;
        globaEVENT_LIB.pos = globaEVENT_LIB.screenPos;
        dwm_send_event_to_all(SURFACE_ID_NONE, EVENT_GLOBAL_MOUSE, &globaEVENT_LIB, sizeof(globaEVENT_LIB));
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
        if (poll1(mouse, POLLIN, 0) != POLLIN)
        {
            break;
        }

        mouse_event_t mouseEvent;
        if (read(mouse, &mouseEvent, sizeof(mouse_event_t)) != sizeof(mouse_event_t))
        {
            printf("dwm: failed to read mouse event\n");
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
    void* newCtx = realloc(pollCtx, sizeof(poll_ctx_t) + (sizeof(pollfd_t) * clientAmount));
    if (newCtx == NULL)
    {
        printf("dwm: failed to realloc pollCtx\n");
        abort();
    }
    else
    {
        pollCtx = newCtx;
    }
    pollCtx->data.fd = data;
    pollCtx->data.events = POLLIN;
    pollCtx->data.revents = 0;
    pollCtx->kbd.fd = kbd;
    pollCtx->kbd.events = POLLIN;
    pollCtx->kbd.revents = 0;
    pollCtx->mouse.fd = mouse;
    pollCtx->mouse.events = POLLIN;
    pollCtx->mouse.revents = 0;

    uint64_t i = 0;
    client_t* client;
    LIST_FOR_EACH(client, &clients, entry)
    {
        pollfd_t* fd = &pollCtx->clients[i++];
        fd->fd = client->fd;
        fd->events = POLLIN;
        fd->revents = 0;
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
    if (events == ERR)
    {
        printf("dwm: poll failed (%s)\n", strerror(errno));
        abort();
    }

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

    if (pollCtx->data.revents & POLLIN)
    {
        dwm_client_accept();
        return; // The clients array is now invalid, so we have to update it.
    }
    if (pollCtx->kbd.revents & POLLIN)
    {
        dwm_kbd_read();
    }
    if (pollCtx->mouse.revents & POLLIN)
    {
        dwm_mouse_read();
    }

    uint64_t i = 0;
    client_t* client;
    client_t* temp;
    LIST_FOR_EACH_SAFE(client, temp, &clients, entry)
    {
        pollfd_t* fd = &pollCtx->clients[i++];
        if (fd->revents & POLLHUP)
        {
            printf("dwm: client %d hung up\n", client->fd);
            dwm_client_disconnect(client);
        }
        else if (fd->revents & POLLERR)
        {
            printf("dwm: client %d error\n", client->fd);
            dwm_client_disconnect(client);
        }
        else if (fd->revents & POLLIN)
        {
            if (client_receive_cmds(client) == ERR)
            {
                printf("dwm: client %d receive commands failed (%s)\n", client->fd, strerror(errno));
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
