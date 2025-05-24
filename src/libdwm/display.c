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
        disp->connected = false;
    }
}

static bool display_events_available(display_t* disp)
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

    disp->connected = true;
    disp->events.readIndex = 0;
    disp->events.writeIndex = 0;
    disp->cmds.amount = 0;
    disp->cmds.size = offsetof(cmd_buffer_t, data);
    list_init(&disp->windows);
    list_init(&disp->fonts);
    list_init(&disp->images);
    disp->newId = SURFACE_ID_NONE - 1;
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

surface_id_t display_gen_id(display_t* disp)
{
    return disp->newId--;
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

bool display_connected(display_t* disp)
{
    return disp->connected;
}

void display_disconnect(display_t* disp)
{
    disp->connected = false;
}

bool display_next_event(display_t* disp, event_t* event, clock_t timeout)
{
    if (display_events_available(disp))
    {
        display_events_pop(disp, event);
        return true;
    }

    if (timeout != CLOCKS_NEVER)
    {
        poll_event_t occurred = poll1(disp->data, POLL_READ, timeout);
        if (occurred & POLL1_ERR)
        {
            disp->connected = false;
            return false;
        }
        else if (!(occurred & POLL_READ))
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
    if (disp->connected && disp->cmds.amount != 0)
    {
        if (write(disp->data, &disp->cmds, disp->cmds.size) != disp->cmds.size)
        {
            disp->connected = false;
        }
    }
    disp->cmds.size = offsetof(cmd_buffer_t, data);
    disp->cmds.amount = 0;
}

uint64_t display_wait_for_event(display_t* disp, event_t* event, event_type_t expected)
{
    while (display_connected(disp))
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
    if (event->target == SURFACE_ID_NONE)
    {
        return;
    }

    window_t* win;
    LIST_FOR_EACH(win, &disp->windows, entry)
    {
        if (event->target == win->surface)
        {
            if (window_dispatch(win, event) == ERR)
            {
                window_free(win);
                disp->connected = false;
            }
            break;
        }
    }

    display_cmds_flush(disp);
}
