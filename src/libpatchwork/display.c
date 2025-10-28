#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

static inline uint64_t display_events_pipe_read(display_t* disp, event_t* event)
{
    if (disp->eventsInPipe > 0)
    {
        if (read(disp->eventsPipe, event, sizeof(event_t)) != sizeof(event_t))
        {
            disp->isConnected = false;
            return ERR;
        }
        disp->eventsInPipe--;
        return sizeof(event_t);
    }

    return 0;
}

static inline uint64_t display_events_pipe_write(display_t* disp, const event_t* event)
{
    if (write(disp->eventsPipe, event, sizeof(event_t)) != sizeof(event_t))
    {
        disp->isConnected = false;
        return ERR;
    }
    disp->eventsInPipe++;
    return sizeof(event_t);
}

display_t* display_new(void)
{
    display_t* disp = malloc(sizeof(display_t));
    if (disp == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    memset(disp->id, 0, MAX_NAME);
    if (readfile("/net/local/seqpacket", disp->id, MAX_NAME, 0) == ERR)
    {
        free(disp);
        return NULL;
    }

    disp->ctl = openf("/net/local/%s/ctl", disp->id);
    if (disp->ctl == ERR)
    {
        free(disp);
        return NULL;
    }
    if (writef(disp->ctl, "connect dwm") == ERR)
    {
        close(disp->ctl);
        free(disp);
        return NULL;
    }

    disp->data = openf("/net/local/%s/data", disp->id);
    if (disp->data == ERR)
    {
        close(disp->ctl);
        free(disp);
        return NULL;
    }

    disp->eventsPipe = open("/dev/pipe/new");
    if (disp->eventsPipe == ERR)
    {
        close(disp->data);
        close(disp->ctl);
        free(disp);
        return NULL;
    }
    disp->eventsInPipe = 0;

    disp->isConnected = true;
    disp->cmds.amount = 0;
    disp->cmds.size = offsetof(cmd_buffer_t, data);
    list_init(&disp->windows);
    list_init(&disp->fonts);
    list_init(&disp->images);

    disp->defaultFont = font_new(disp, "default", "regular", 16);
    if (disp->defaultFont == NULL)
    {
        close(disp->eventsPipe);
        close(disp->data);
        close(disp->ctl);
        free(disp);
        return NULL;
    }

    if (mtx_init(&disp->mutex, mtx_recursive) == thrd_error)
    {
        font_free(disp->defaultFont);
        close(disp->eventsPipe);
        close(disp->data);
        close(disp->ctl);
        free(disp);
        return NULL;
    }

    return disp;
}

void display_free(display_t* disp)
{
    if (disp == NULL)
    {
        return;
    }

    window_t* window;
    window_t* temp1;
    LIST_FOR_EACH_SAFE(window, temp1, &disp->windows, entry)
    {
        window_free(window);
    }

    font_t* font;
    font_t* temp2;
    LIST_FOR_EACH_SAFE(font, temp2, &disp->fonts, entry)
    {
        font_free(font);
    }

    image_t* image;
    image_t* temp3;
    LIST_FOR_EACH_SAFE(image, temp3, &disp->images, entry)
    {
        image_free(image);
    }

    close(disp->eventsPipe);
    close(disp->ctl);
    close(disp->data);
    mtx_destroy(&disp->mutex);
    free(disp);
}

bool display_is_connected(display_t* disp)
{
    if (disp == NULL)
    {
        return false;
    }

    mtx_lock(&disp->mutex);
    bool temp = disp->isConnected;
    mtx_unlock(&disp->mutex);
    return temp;
}

void display_disconnect(display_t* disp)
{
    if (disp == NULL)
    {
        return;
    }

    mtx_lock(&disp->mutex);
    disp->isConnected = false;
    mtx_unlock(&disp->mutex);
}

void* display_cmd_alloc(display_t* disp, cmd_type_t type, uint64_t size)
{
    if (disp == NULL || size > CMD_BUFFER_MAX_DATA)
    {
        errno = EINVAL;
        return NULL;
    }

    mtx_lock(&disp->mutex);
    if (disp->cmds.size + size >= CMD_BUFFER_MAX_DATA)
    {
        display_cmds_flush(disp);
    }

    cmd_header_t* cmd = (cmd_header_t*)((uint64_t)&disp->cmds + disp->cmds.size);
    disp->cmds.amount++;
    disp->cmds.size += size;

    cmd->magic = CMD_MAGIC;
    cmd->type = type;
    cmd->size = size;
    mtx_unlock(&disp->mutex);
    return cmd;
}

void display_cmds_flush(display_t* disp)
{
    mtx_lock(&disp->mutex);
    if (disp->isConnected && disp->cmds.amount != 0)
    {
        if (write(disp->data, &disp->cmds, disp->cmds.size) != disp->cmds.size)
        {
            disp->isConnected = false;
        }
    }
    disp->cmds.size = offsetof(cmd_buffer_t, data);
    disp->cmds.amount = 0;
    mtx_unlock(&disp->mutex);
}

uint64_t display_next(display_t* disp, event_t* event, clock_t timeout)
{
    if (disp == NULL || event == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    mtx_lock(&disp->mutex);
    if (!disp->isConnected)
    {
        mtx_unlock(&disp->mutex);
        errno = ENOTCONN;
        return ERR;
    }
    uint64_t readBytes = display_events_pipe_read(disp, event);
    mtx_unlock(&disp->mutex);
    if (readBytes == sizeof(event_t))
    {
        return 0;
    }
    else if (readBytes == ERR)
    {
        return ERR;
    }

    pollfd_t fds[] = {{.fd = disp->data, .events = POLLIN}, {.fd = disp->eventsPipe, .events = POLLIN}};
    poll_events_t revents = poll(fds, 2, timeout);
    if (revents & POLLERR)
    {
        display_disconnect(disp);
        return ERR;
    }
    else if (!(revents & POLLIN))
    {
        errno = ETIMEDOUT;
        return ERR;
    }

    mtx_lock(&disp->mutex);
    if (!disp->isConnected)
    {
        mtx_unlock(&disp->mutex);
        errno = ENOTCONN;
        return ERR;
    }
    if (fds[1].revents & POLLIN)
    {
        readBytes = display_events_pipe_read(disp, event);
        if (readBytes == sizeof(event_t))
        {
            mtx_unlock(&disp->mutex);
            return 0;
        }
        else if (readBytes == ERR)
        {
            mtx_unlock(&disp->mutex);
            return ERR;
        }
    }
    if (fds[0].revents & POLLIN)
    {
        if (read(disp->data, event, sizeof(event_t)) != sizeof(event_t))
        {
            disp->isConnected = false;
            mtx_unlock(&disp->mutex);
            return ERR;
        }
        mtx_unlock(&disp->mutex);
        return 0;
    }
    mtx_unlock(&disp->mutex);
    return 0;
}

uint64_t display_poll(display_t* disp, pollfd_t* fds, uint64_t nfds, clock_t timeout)
{
    if (disp == NULL || fds == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (!display_is_connected(disp))
    {
        errno = ENOTCONN;
        return ERR;
    }

    pollfd_t* allFds = malloc(sizeof(pollfd_t) * (nfds + 2));
    if (allFds == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }

    allFds[0].fd = disp->data;
    allFds[0].events = POLLIN;
    allFds[1].fd = disp->eventsPipe;
    allFds[1].events = POLLIN;
    for (uint64_t i = 0; i < nfds; i++)
    {
        allFds[i + 2] = fds[i];
    }

    uint64_t ready = poll(allFds, nfds + 2, timeout);
    if (ready == ERR)
    {
        free(allFds);
        return ERR;
    }

    if (allFds[0].revents & POLLERR || allFds[1].revents & POLLERR)
    {
        display_disconnect(disp);
        free(allFds);
        return ERR;
    }

    uint64_t totalReady = ready;
    if (allFds[0].revents & POLLIN)
    {
        totalReady--;
    }
    if (allFds[1].revents & POLLIN)
    {
        totalReady--;
    }
    for (uint64_t i = 0; i < nfds; i++)
    {
        fds[i].revents = allFds[i + 2].revents;
    }
    free(allFds);
    return totalReady;
}

void display_push(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size)
{
    if (disp == NULL || (data == NULL && size > 0) || size > EVENT_MAX_DATA)
    {
        return;
    }

    event_t event = {
        .target = target,
        .type = type,
    };
    memcpy(event.raw, data, MIN(EVENT_MAX_DATA, size));

    mtx_lock(&disp->mutex);
    display_events_pipe_write(disp, &event);
    mtx_unlock(&disp->mutex);
}

uint64_t display_wait(display_t* disp, event_t* event, event_type_t expected)
{
    if (disp == NULL || event == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    mtx_lock(&disp->mutex);
    uint64_t initEventsInPipe = disp->eventsInPipe;
    for (uint64_t i = 0; i < initEventsInPipe; i++)
    {
        if (display_events_pipe_read(disp, event) == ERR)
        {
            mtx_unlock(&disp->mutex);
            return ERR;
        }

        if (event->type != expected)
        {
            display_events_pipe_write(disp, event);
        }
        else
        {
            mtx_unlock(&disp->mutex);
            return 0;
        }
    }

    while (true)
    {
        if (read(disp->data, event, sizeof(event_t)) != sizeof(event_t))
        {
            disp->isConnected = false;
            mtx_unlock(&disp->mutex);
            return ERR;
        }

        if (event->type == expected)
        {
            mtx_unlock(&disp->mutex);
            return 0;
        }
        else
        {
            mtx_lock(&disp->mutex);
            display_events_pipe_write(disp, event);
            mtx_unlock(&disp->mutex);
        }
    }
}

uint64_t display_emit(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size)
{
    if (disp == NULL || (data == NULL && size > 0) || size > EVENT_MAX_DATA)
    {
        errno = EINVAL;
        return ERR;
    }

    event_t event = {
        .target = target,
        .type = type,
    };
    memcpy(event.raw, data, MIN(EVENT_MAX_DATA, size));
    if (display_dispatch(disp, &event) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t display_dispatch(display_t* disp, const event_t* event)
{
    if (disp == NULL || event == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    mtx_lock(&disp->mutex);
    window_t* win;
    window_t* temp;
    LIST_FOR_EACH_SAFE(win, temp, &disp->windows, entry)
    {
        if (event->target == win->surface || event->target == SURFACE_ID_NONE)
        {
            if (window_dispatch(win, event) == ERR)
            {
                disp->isConnected = false;
            }

            if (event->target == win->surface)
            {
                break;
            }
        }
    }

    display_cmds_flush(disp);
    mtx_unlock(&disp->mutex);
    return 0;
}

uint64_t display_dispatch_pending(display_t* disp, event_type_t type, surface_id_t target)
{
    if (disp == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    mtx_lock(&disp->mutex);
    uint64_t initEventsInPipe = disp->eventsInPipe;
    for (uint64_t i = 0; i < initEventsInPipe; i++)
    {
        event_t event;
        if (display_events_pipe_read(disp, &event) == ERR)
        {
            mtx_unlock(&disp->mutex);
            return ERR;
        }

        if (event.type == type && (event.target == target || target == SURFACE_ID_NONE))
        {
            if (display_dispatch(disp, &event) == ERR)
            {
                mtx_unlock(&disp->mutex);
                return ERR;
            }
        }
        else
        {
            display_events_pipe_write(disp, &event);
        }
    }
    mtx_unlock(&disp->mutex);

    return 0;
}

uint64_t display_subscribe(display_t* disp, event_type_t type)
{
    if (disp == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    cmd_subscribe_t* cmd = display_cmd_alloc(disp, CMD_SUBSCRIBE, sizeof(cmd_subscribe_t));
    if (cmd == NULL)
    {
        mtx_unlock(&disp->mutex);
        return ERR;
    }
    cmd->event = type;
    display_cmds_flush(disp);
    return 0;
}

uint64_t display_unsubscribe(display_t* disp, event_type_t type)
{
    if (disp == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    cmd_unsubscribe_t* cmd = display_cmd_alloc(disp, CMD_UNSUBSCRIBE, sizeof(cmd_unsubscribe_t));
    if (cmd == NULL)
    {
        mtx_unlock(&disp->mutex);
        return ERR;
    }
    cmd->event = type;
    display_cmds_flush(disp);
    return 0;
}

uint64_t display_get_surface_info(display_t* disp, surface_id_t id, surface_info_t* info)
{
    if (disp == NULL || info == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    cmd_surface_report_t* cmd = display_cmd_alloc(disp, CMD_SURFACE_REPORT, sizeof(cmd_surface_report_t));
    if (cmd == NULL)
    {
        mtx_unlock(&disp->mutex);
        return ERR;
    }
    cmd->isGlobal = true;
    cmd->target = id;
    display_cmds_flush(disp);

    event_t event;
    if (display_wait(disp, &event, EVENT_REPORT) == ERR)
    {
        return ERR;
    }
    *info = event.report.info;
    return 0;
}

uint64_t display_set_focus(display_t* disp, surface_id_t id)
{
    cmd_surface_focus_set_t* cmd = display_cmd_alloc(disp, CMD_SURFACE_FOCUS_SET, sizeof(cmd_surface_focus_set_t));
    if (cmd == NULL)
    {
        mtx_unlock(&disp->mutex);
        return ERR;
    }
    cmd->isGlobal = true;
    cmd->target = id;
    display_cmds_flush(disp);
    return 0;
}

uint64_t display_set_is_visible(display_t* disp, surface_id_t id, bool isVisible)
{
    cmd_surface_visible_set_t* cmd =
        display_cmd_alloc(disp, CMD_SURFACE_VISIBLE_SET, sizeof(cmd_surface_visible_set_t));
    if (cmd == NULL)
    {
        return ERR;
    }
    cmd->isGlobal = true;
    cmd->target = id;
    cmd->isVisible = isVisible;
    display_cmds_flush(disp);
    return 0;
}

uint64_t display_get_screen(display_t* disp, rect_t* rect, uint64_t index)
{
    if (disp == NULL || rect == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    cmd_screen_info_t* cmd = display_cmd_alloc(disp, CMD_SCREEN_INFO, sizeof(cmd_screen_info_t));
    if (cmd == NULL)
    {
        return ERR;
    }
    cmd->index = index;
    display_cmds_flush(disp);

    event_t event;
    if (display_wait(disp, &event, EVENT_SCREEN_INFO) == ERR)
    {
        return ERR;
    }
    *rect = RECT_INIT(0, 0, event.screenInfo.width, event.screenInfo.height);
    return 0;
}
