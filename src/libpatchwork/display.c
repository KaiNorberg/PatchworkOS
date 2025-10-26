#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

static void display_receive_event(display_t* disp, event_t* event)
{
    uint64_t result;
    do
    {
        result = read(disp->data, event, sizeof(event_t));
    } while (result == ERR && errno == EINTR);

    if (result == sizeof(event_t))
    {
        return;
    }

    disp->isConnected = false;
}

static bool display_is_events_avail(display_t* disp)
{
    return disp->events.readIndex != disp->events.writeIndex;
}

static void display_events_pop(display_t* disp, event_t* event)
{
    *event = disp->events.buffer[disp->events.readIndex];
    disp->events.readIndex = (disp->events.readIndex + 1) % DISPLAY_MAX_EVENT;
}

display_t* display_new(void)
{
    display_t* disp = malloc(sizeof(display_t));
    if (disp == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

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

    disp->isConnected = true;
    disp->events.readIndex = 0;
    disp->events.writeIndex = 0;
    disp->cmds.amount = 0;
    disp->cmds.size = offsetof(cmd_buffer_t, data);
    list_init(&disp->windows);
    list_init(&disp->fonts);
    list_init(&disp->images);

    disp->defaultFont = font_new(disp, "default", "regular", 16);
    if (disp->defaultFont == NULL)
    {
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

    close(disp->ctl);
    close(disp->data);
    free(disp);
}

rect_t display_screen_rect(display_t* disp, uint64_t index)
{
    if (disp == NULL)
    {
        return RECT_INIT(0, 0, 0, 0);
    }

    cmd_screen_info_t* cmd = display_cmd_alloc(disp, CMD_SCREEN_INFO, sizeof(cmd_screen_info_t));
    if (cmd == NULL)
    {
        return RECT_INIT(0, 0, 0, 0);
    }
    cmd->index = index;
    display_cmds_flush(disp);

    event_t event;
    if (display_wait_for_event(disp, &event, EVENT_SCREEN_INFO) == ERR)
    {
        return RECT_INIT(0, 0, 0, 0);
    }
    return RECT_INIT(0, 0, event.screenInfo.width, event.screenInfo.height);
}

fd_t display_data_fd(display_t* disp)
{
    if (disp == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    return disp->data;
}

bool display_is_connected(display_t* disp)
{
    if (disp == NULL)
    {
        return false;
    }

    return disp->isConnected;
}

void display_disconnect(display_t* disp)
{
    if (disp == NULL)
    {
        return;
    }

    disp->isConnected = false;
}

void* display_cmd_alloc(display_t* disp, cmd_type_t type, uint64_t size)
{
    if (disp == NULL || size > CMD_BUFFER_MAX_DATA)
    {
        errno = EINVAL;
        return NULL;
    }

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
    return cmd;
}

void display_cmds_flush(display_t* disp)
{
    if (disp->isConnected && disp->cmds.amount != 0)
    {
        if (write(disp->data, &disp->cmds, disp->cmds.size) != disp->cmds.size)
        {
            disp->isConnected = false;
        }
    }
    disp->cmds.size = offsetof(cmd_buffer_t, data);
    disp->cmds.amount = 0;
}

uint64_t display_next_event(display_t* disp, event_t* event, clock_t timeout)
{
    if (disp == NULL || event == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (display_is_events_avail(disp))
    {
        display_events_pop(disp, event);
        return 0;
    }

    if (timeout != CLOCKS_NEVER)
    {
        poll_events_t revents = poll1(disp->data, POLLIN, timeout);
        if (revents & POLLERR)
        {
            disp->isConnected = false;
            return ERR;
        }
        else if (!(revents & POLLIN))
        {
            errno = ETIMEDOUT;
            return ERR;
        }
    }

    display_receive_event(disp, event);
    if (!display_is_connected(disp))
    {
        errno = ENOTCONN;
        return ERR;
    }
    return 0;
}

void display_events_push(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size)
{
    if (disp == NULL || (data == NULL && size > 0) || size > EVENT_MAX_DATA)
    {
        return;
    }

    uint64_t nextWriteIndex = (disp->events.writeIndex + 1) % DISPLAY_MAX_EVENT;
    if (nextWriteIndex == disp->events.readIndex)
    {
        disp->events.readIndex = (disp->events.readIndex + 1) % DISPLAY_MAX_EVENT;
    }

    event_t* event = &disp->events.buffer[disp->events.writeIndex];
    event->type = type;
    event->target = target;
    memcpy(event->raw, data, size);

    disp->events.writeIndex = nextWriteIndex;
}

uint64_t display_wait_for_event(display_t* disp, event_t* event, event_type_t expected)
{
    if (disp == NULL || event == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t initReadIndex = disp->events.readIndex;
    while (display_is_events_avail(disp))
    {
        display_events_pop(disp, event);
        if (event->type == expected)
        {
            return 0;
        }
        else
        {
            display_events_push(disp, event->target, event->type, event->raw, EVENT_MAX_DATA);
        }

        if (disp->events.readIndex == initReadIndex)
        {
            break;
        }
    }

    while (true)
    {
        display_receive_event(disp, event);
        if (!display_is_connected(disp))
        {
            errno = ENOTCONN;
            return ERR;
        }

        if (event->type != expected)
        {
            display_events_push(disp, event->target, event->type, event->raw, EVENT_MAX_DATA);
        }
        else
        {
            return 0;
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
        return ERR;
    }
    cmd->isGlobal = true;
    cmd->target = id;
    display_cmds_flush(disp);

    event_t event;
    if (display_wait_for_event(disp, &event, EVENT_REPORT) == ERR)
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
