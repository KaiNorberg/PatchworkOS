#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

window_t* window_new(display_t* disp, window_t* parent, const char* name, const rect_t* rect, surface_id_t id, surface_type_t type, window_flags_t flags, procedure_t procedure)
{
    if (strlen(name) >= MAX_NAME)
    {
       return NULL;
    }

    window_t* win = malloc(sizeof(window_t));
    if (win == NULL)
    {
        return NULL;
    }

    cmd_t cmd;
    cmd.type = CMD_SURFACE_NEW;
    cmd.surfaceNew.id = id;
    cmd.surfaceNew.parent = parent != NULL ? parent->id : SURFACE_ID_ROOT;
    cmd.surfaceNew.type = type;
    cmd.surfaceNew.rect = *rect;
    display_cmds_push(disp, &cmd);
    display_cmds_flush(disp);

    list_entry_init(&win->entry);
    win->id = id;
    strcpy(win->name, name);
    win->rect = *rect;
    win->type = type;
    win->flags = flags;
    win->proc = procedure;
    win->display = disp;
    list_push(&disp->windows, &win->entry);
    return win;
}

void window_free(window_t* win)
{
    cmd_t cmd = {.type = CMD_SURFACE_FREE, .surfaceFree.target = win->id};
    display_cmds_push(win->display, &cmd);
    display_cmds_flush(win->display);
    free(win);
}

void window_get_rect(window_t* window, rect_t* rect)
{
    *rect = window->rect;
}

void window_draw_rect(window_t* win, const rect_t* rect, pixel_t pixel)
{
    cmd_t cmd = {.type = CMD_DRAW_RECT, .drawRect = {.target = win->id, .rect = *rect, .pixel = pixel}};
    display_cmds_push(win->display, &cmd);
}
