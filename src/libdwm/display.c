#include "internal.h"

#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

static bool display_events_available(display_t* disp)
{
    return disp->events.readIndex != disp->events.writeIndex;
}

static void display_events_push(display_t* disp, event_t* event)
{
    uint64_t nextWriteIndex = (disp->events.writeIndex + 1) % DISPLAY_MAX_EVENT;
    if (nextWriteIndex == disp->events.readIndex)
    {
        disp->events.readIndex = (disp->events.readIndex + 1) % DISPLAY_MAX_EVENT;
    }

    disp->events.buffer[disp->events.writeIndex] = *event;
    disp->events.writeIndex = nextWriteIndex;
}

static void display_events_pop(display_t* disp, event_t* event)
{
    *event = disp->events.buffer[disp->events.readIndex];
    disp->events.readIndex = (disp->events.readIndex + 1) % DISPLAY_MAX_EVENT;
}

static void display_recieve_event(display_t* disp, event_t* event)
{
    if (read(disp->data, event, sizeof(event_t)) != sizeof(event_t))
    {
        disp->connected = false;
    }
}

display_t* display_open(void)
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
    disp->newId = SURFACE_ID_NONE - 1;
    return disp;
}

void display_close(display_t* disp)
{
    window_t* window;
    window_t* temp;
    LIST_FOR_EACH_SAFE(window, temp, &disp->windows, entry)
    {
        window_free(window);
    }

    close(disp->handle);
    close(disp->data);
    free(disp);
}

surface_id_t display_gen_id(display_t* disp)
{
    return disp->newId--;
}

void display_screen_rect(display_t* disp, rect_t* rect, uint64_t index)
{
    cmd_screen_info_t cmd;
    CMD_INIT(&cmd, CMD_SCREEN_INFO, cmd_screen_info_t);
    cmd.index = index;

    event_t event;
    if (display_send_recieve_pattern(disp, &cmd.header, &event, EVENT_SCREEN_INFO) == ERR)
    {
        rect->left = 0;
        rect->top = 0;
        rect->right = 0;
        rect->bottom = 0;
        return;
    }

    event_screen_info_t* screenInfo = (event_screen_info_t*)event.data;
    rect->left = 0;
    rect->top = 0;
    rect->right = screenInfo->width;
    rect->bottom = screenInfo->height;
}

bool display_connected(display_t* disp)
{
    return disp->connected;
}

bool display_next_event(display_t* disp, event_t* event, nsec_t timeout)
{
    if (display_events_available(disp))
    {
        display_events_pop(disp, event);
        return true;
    }

    if (timeout != NEVER && poll1(disp->data, POLL_READ, timeout) != POLL_READ)
    {
        return false;
    }

    display_recieve_event(disp, event);
    return true;
}

void display_dispatch(display_t* disp, event_t* event)
{
    if (event->target == SURFACE_ID_NONE)
    {
        return;
    }

    window_t* win;
    LIST_FOR_EACH(win, &disp->windows, entry)
    {
        if (event->target == win->id)
        {
            if (window_dispatch(win, event) == ERR)
            {
                window_free(win);
            }
            break;
        }
    }

    display_cmds_flush(disp);
}

uint64_t display_send_recieve_pattern(display_t* disp, cmd_header_t* cmd, event_t* event, event_type_t expected)
{
    display_cmds_push(disp, cmd);
    display_cmds_flush(disp);

    while (display_connected(disp))
    {
        display_recieve_event(disp, event);

        if (event->type != expected)
        {
            display_events_push(disp, event);
        }
        else
        {
            return 0;
        }
    }

    return ERR;
}

void display_cmds_push(display_t* disp, const cmd_header_t* cmd)
{
    if (disp->cmds.size + cmd->size >= CMD_BUFFER_MAX_DATA - offsetof(cmd_buffer_t, data))
    {
        display_cmds_flush(disp);
    }

    memcpy((void*)((uint64_t)&disp->cmds + disp->cmds.size), cmd, cmd->size);
    disp->cmds.amount++;
    disp->cmds.size += cmd->size;
}

void display_cmds_flush(display_t* disp)
{
    if (write(disp->data, &disp->cmds, disp->cmds.size) != disp->cmds.size)
    {
        disp->connected = false;
    }
    disp->cmds.size = offsetof(cmd_buffer_t, data);
    disp->cmds.amount = 0;
}
