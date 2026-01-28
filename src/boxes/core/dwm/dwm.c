#include "dwm.h"

#include "client.h"
#include "compositor.h"
#include "kbd.h"
#include "screen.h"
#include "surface.h"

#include <errno.h>
#include <patchwork/event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>
#include <threads.h>

static char* id;
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
    fd_t fd;
    if (IS_ERR(open(&fd, F("/net/local/%s/accept:nonblock", id))))
    {
        printf("dwm: failed to open accept file\n");
        return NULL;
    }

    client_t* client = client_new(fd);
    if (client == NULL)
    {
        printf("dwm: failed to accept client\n");
        close(fd);
        return NULL;
    }

    list_push_back(&clients, &client->entry);
    clientAmount++;
    printf("dwm: accepted client %d total %lu\n", client->fd, clientAmount);
    return client;
}

static void dwm_client_disconnect(client_t* client)
{
    list_remove(&client->entry);
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
        if (IS_ERR(client_send_event(client, target, type, data, size)))
        {
            dwm_client_disconnect(client);
        }
    }
}

void dwm_init(void)
{
    fd_t klog;
    if (IS_ERR(open(&klog, "/dev/klog")))
    {
        abort();
    }

    fd_t stdoutFd = STDOUT_FILENO;
    if (IS_ERR(dup(klog, &stdoutFd)))
    {
        close(klog);
        abort();
    }
    close(klog);

    if (IS_ERR(open(&kbd, "/dev/kbd/0/events:nonblock")))
    {
        printf("dwm: failed to open keyboard\n");
        abort();
    }

    char* name;
    if (!IS_ERR(readfiles(&name, "/dev/kbd/0/name")))
    {
        printf("dwm: using keyboard '%s'\n", name);
        free(name);
    }

    if (IS_ERR(open(&mouse, "/dev/mouse/0/events:nonblock")))
    {
        printf("dwm: failed to open mouse\n");
        abort();
    }

    if (!IS_ERR(readfiles(&name, "/dev/mouse/0/name")))
    {
        printf("dwm: using mouse '%s'\n", name);
        free(name);
    }

    if (IS_ERR(readfiles(&id, "/net/local/seqpacket:nonblock")))
    {
        printf("dwm: failed to read seqpacket id\n");
        abort();
    }

    if (IS_ERR(writefiles(F("/net/local/%s/ctl", id), "bind dwm && listen")))
    {
        printf("dwm: failed to bind socket\n");
        abort();
    }

    if (IS_ERR(open(&data, F("/net/local/%s/data:nonblock", id))))
    {
        printf("dwm: failed to open data file\n");
        abort();
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
        list_push_back(&windows, &surface->dwmEntry);
    }
    break;
    case SURFACE_PANEL:
    {
        list_push_back(&panels, &surface->dwmEntry);
    }
    break;
    case SURFACE_CURSOR:
    {
        if (cursor != NULL)
        {
            printf("dwm: attach (cursor != NULL)\n");
            errno = EALREADY;
            return -1;
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
            return -1;
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
            return -1;
        }

        fullscreen = surface;
        focus = surface;
    }
    break;
    default:
    {
        printf("dwm: attach (default)\n");
        errno = EINVAL;
        return -1;
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
        list_remove(&surface->dwmEntry);
    }
    break;
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
            list_remove(&surface->dwmEntry);
            list_push_back(&windows, &surface->dwmEntry);
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

/// @todo Optimize this function to avoid iterating over all surfaces every time.
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
    static kbd_mods_t mods = KBD_MOD_NONE;

    keycode_t code = -1;
    char suffix = -1;
    uint64_t result = scan(kbd, "%u%c", &code, &suffix);
    if (result != 2)
    {
        printf("dwm: failed to read keyboard event\n");
        return;
    }

    code = kbd_translate(code);

    if (code == KBD_LEFT_SHIFT || code == KBD_RIGHT_SHIFT)
    {
        if (suffix == '_')
        {
            mods |= KBD_MOD_SHIFT;
        }
        else
        {
            mods &= ~KBD_MOD_SHIFT;
        }
    }
    else if (code == KBD_LEFT_CTRL || code == KBD_RIGHT_CTRL)
    {
        if (suffix == '_')
        {
            mods |= KBD_MOD_CTRL;
        }
        else
        {
            mods &= ~KBD_MOD_CTRL;
        }
    }
    else if (code == KBD_LEFT_ALT || code == KBD_RIGHT_ALT)
    {
        if (suffix == '_')
        {
            mods |= KBD_MOD_ALT;
        }
        else
        {
            mods &= ~KBD_MOD_ALT;
        }
    }
    else if (code == KBD_LEFT_SUPER || code == KBD_RIGHT_SUPER)
    {
        if (suffix == '_')
        {
            mods |= KBD_MOD_SUPER;
        }
        else
        {
            mods &= ~KBD_MOD_SUPER;
        }
    }
    else if (code == KBD_CAPS_LOCK && suffix == '_')
    {
        mods ^= KBD_MOD_CAPS;
    }

    if (focus != NULL)
    {
        event_kbd_t event;
        event.type = suffix == '_' ? KBD_PRESS : KBD_RELEASE;
        event.mods = mods;
        event.code = code;
        event.ascii = kbd_ascii(code, mods);
        client_send_event(focus->client, focus->id, EVENT_KBD, &event, sizeof(event_kbd_t));

        event_global_kbd_t globalEvent;
        globalEvent.type = event.type;
        globalEvent.mods = event.mods;
        globalEvent.code = event.code;
        globalEvent.ascii = event.ascii;
        dwm_send_event_to_all(SURFACE_ID_NONE, EVENT_GLOBAL_KBD, &globalEvent, sizeof(globalEvent));
    }
}

static void dwm_handle_mouse_event(int64_t x, int64_t y, mouse_buttons_t buttons)
{
    static mouse_buttons_t prevHeld = MOUSE_NONE;

    if (cursor == NULL)
    {
        return;
    }

    mouse_buttons_t held = buttons;
    mouse_buttons_t pressed = buttons & ~prevHeld;
    mouse_buttons_t released = prevHeld & ~buttons;

    point_t oldCursorPos = cursor->pos;
    cursor->pos.x = CLAMP(cursor->pos.x + x, 0, (int64_t)screen_width() - 1);
    cursor->pos.y = CLAMP(cursor->pos.y + y, 0, (int64_t)screen_height() - 1);

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

        event_global_mouse_t globalEvent = event;
        globalEvent.pos = globalEvent.screenPos;
        dwm_send_event_to_all(SURFACE_ID_NONE, EVENT_GLOBAL_MOUSE, &globalEvent, sizeof(globalEvent));
    }

    prevHeld = held;
}

static void dwm_mouse_read(void)
{
    static mouse_buttons_t buttons = MOUSE_NONE;

    int64_t x = 0;
    int64_t y = 0;
    while (1)
    {
        int64_t value;
        char suffix;
        if (scan(mouse, "%lld%c", &value, &suffix) != 2)
        {
            if (errno != EAGAIN)
            {
                printf("dwm: failed to read mouse event\n");
            }
            break;
        }

        switch (suffix)
        {
        case 'x':
            x += value;
            break;
        case 'y':
            y += value;
            break;
        case '_':
            if (x != 0 || y != 0)
            {
                dwm_handle_mouse_event(x, y, buttons);
                x = 0;
                y = 0;
            }
            buttons |= (1 << value);
            dwm_handle_mouse_event(0, 0, buttons);
            break;
        case '^':
            if (x != 0 || y != 0)
            {
                dwm_handle_mouse_event(x, y, buttons);
                x = 0;
                y = 0;
            }
            buttons &= ~(1 << value);
            dwm_handle_mouse_event(0, 0, buttons);
            break;
        default:
            printf("dwm: unknown mouse event suffix '%c'\n", suffix);
            break;
        }
    }

    if (x != 0 || y != 0)
    {
        dwm_handle_mouse_event(x, y, buttons);
    }
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

    uint64_t count;
    status_t status = poll((pollfd_t*)pollCtx, sizeof(poll_ctx_t) / sizeof(pollfd_t) + clientAmount, timeout, &count);
    if (IS_ERR(status))
    {
        printf("dwm: poll failed\n");
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
            if (IS_ERR(client_receive_cmds(client)))
            {
                printf("dwm: client %d receive commands failed\n", client->fd);
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
