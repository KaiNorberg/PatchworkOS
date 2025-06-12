#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

static void display_receive_event(display_t* disp, event_t* event)
{
    if (read(disp->data, event, sizeof(event_t)) != sizeof(event_t))
    {
        disp->isConnected = false;
    }
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

        return NULL;
    }

    disp->handle = open("sys:/net/local/new");
    if (disp->handle == ERR)
    {
        free(disp);
        return NULL;
    }
    read(disp->handle, disp->id, MAX_NAME);

    fd_t ctl = openf("sys:/net/local/%s/ctl", disp->id);
    if (ctl == ERR)
    {
        close(disp->handle);
        free(disp);
        return NULL;
    }
    if (writef(ctl, "connect dwm") == ERR)
    {
        close(ctl);
        close(disp->handle);
        free(disp);
        return NULL;
    }
    close(ctl);

    disp->data = openf("sys:/net/local/%s/data", disp->id);
    if (disp->data == ERR)
    {
        close(disp->handle);
        free(disp);
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
        close(disp->handle);
        close(disp->data);
        free(disp);
        return NULL;
    }

    return disp;
}

void display_free(display_t* disp)
{
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

    close(disp->handle);
    close(disp->data);
    free(disp);
}

uint64_t display_screen_rect(display_t* disp, rect_t* rect, uint64_t index)
{
    cmd_screen_info_t* cmd = display_cmds_push(disp, CMD_SCREEN_INFO, sizeof(cmd_screen_info_t));
    cmd->index = index;
    display_cmds_flush(disp);

    event_t event;
    if (display_wait_for_event(disp, &event, EVENT_SCREEN_INFO) == ERR)
    {
        return ERR;
    }

    rect->left = 0;
    rect->top = 0;
    rect->right = event.screenInfo.width;
    rect->bottom = event.screenInfo.height;
    return 0;
}

fd_t display_fd(display_t* disp)
{
    return disp->data;
}

bool display_is_connected(display_t* disp)
{
    return disp->isConnected;
}

void display_disconnect(display_t* disp)
{
    disp->isConnected = false;
}

bool display_next_event(display_t* disp, event_t* event, clock_t timeout)
{
    if (display_is_events_avail(disp))
    {
        display_events_pop(disp, event);
        return true;
    }

    if (timeout != CLOCKS_NEVER)
    {
        poll_events_t revents = poll1(disp->data, POLL_READ, timeout);
        if (revents & POLL1_ERR)
        {
            disp->isConnected = false;
            return false;
        }
        else if (!(revents & POLL_READ))
        {
            return false;
        }
    }

    display_receive_event(disp, event);
    return true;
}

void display_events_push(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size)
{
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

void* display_cmds_push(display_t* disp, cmd_type_t type, uint64_t size)
{
    if (size > CMD_BUFFER_MAX_DATA)
    {
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

uint64_t display_wait_for_event(display_t* disp, event_t* event, event_type_t expected)
{
    while (display_is_connected(disp))
    {
        display_receive_event(disp, event);

        if (event->type != expected)
        {
            display_events_push(disp, event->target, event->type, event->raw, EVENT_MAX_DATA);
        }
        else
        {
            return 0;
        }
    }

    return ERR;
}

void display_emit(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size)
{
    event_t event = {
        .target = target,
        .type = type,
    };
    memcpy(event.raw, data, MIN(EVENT_MAX_DATA, size));
    display_dispatch(disp, &event);
}

void display_dispatch(display_t* disp, const event_t* event)
{
    window_t* win;
    window_t* temp;
    LIST_FOR_EACH_SAFE(win, temp, &disp->windows, entry)
    {
        if (event->target == win->surface || event->target == SURFACE_ID_NONE)
        {
            if (window_dispatch(win, event) == ERR)
            {
                window_free(win);
                disp->isConnected = false;
            }
            break;
        }
    }

    display_cmds_flush(disp);
}

void display_subscribe(display_t* disp, event_type_t type)
{
    cmd_subscribe_t* cmd = display_cmds_push(disp, CMD_SUBSCRIBE, sizeof(cmd_subscribe_t));
    cmd->event = type;
    display_cmds_flush(disp);
}

void display_unsubscribe(display_t* disp, event_type_t type)
{
    cmd_unsubscribe_t* cmd = display_cmds_push(disp, CMD_UNSUBSCRIBE, sizeof(cmd_unsubscribe_t));
    cmd->event = type;
    display_cmds_flush(disp);
}

void display_get_surface_info(display_t* disp, surface_id_t id, surface_info_t* info)
{
    cmd_surface_report_t* cmd = display_cmds_push(disp, CMD_SURFACE_REPORT, sizeof(cmd_surface_report_t));
    cmd->isGlobal = true;
    cmd->target = id;
    display_cmds_flush(disp);

    event_t event;
    display_wait_for_event(disp, &event, EVENT_REPORT);
    *info = event.report.info;
}

void display_set_visible(display_t* disp, surface_id_t id)
{
    cmd_surface_focus_set_t* cmd = display_cmds_push(disp, CMD_SURFACE_FOCUS_SET, sizeof(cmd_surface_focus_set_t));
    cmd->isGlobal = true;
    cmd->target = id;
    display_cmds_flush(disp);
}

void display_set_is_visible(display_t* disp, surface_id_t id, bool isVisible)
{
    cmd_surface_visible_set_t* cmd =
        display_cmds_push(disp, CMD_SURFACE_VISIBLE_SET, sizeof(cmd_surface_visible_set_t));
    cmd->isGlobal = true;
    cmd->target = id;
    cmd->isVisible = isVisible;
    display_cmds_flush(disp);
}
