#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: this should be stored in some sort of config file, lua? make something custom?
#define WIN_DEFAULT_FONT "home:/theme/fonts/zap-vga16.psf"
window_theme_t windowTheme = {
    .edgeWidth = 3,
    .rimWidth = 3,
    .ridgeWidth = 2,
    .highlight = 0xFFE0E0E0,
    .shadow = 0xFF6F6F6F,
    .bright = 0xFFFFFFFF,
    .dark = 0xFF000000,
    .background = 0xFFBFBFBF,
    .selected = 0xFF00007F,
    .selectedHighlight = 0xFF2186CD,
    .unSelected = 0xFF7F7F7F,
    .unSelectedHighlight = 0xFFAFAFAF,
    .topbarHeight = 40,
    .paddingWidth = 2,
};

static uint64_t window_deco_procedure(window_t* win, const event_t* event)
{
    switch (event->type)
    {
    case EVENT_INIT:
    {
        printf("deco: init");
    }
    break;
    case EVENT_REDRAW:
    {
        rect_t rect;
        window_get_local_rect(win, &rect);

        window_draw_edge(win, &rect, windowTheme.edgeWidth, windowTheme.bright, windowTheme.dark);

        rect_t topBar = (rect_t){
            .left = windowTheme.edgeWidth + windowTheme.paddingWidth,
            .top = windowTheme.edgeWidth + windowTheme.paddingWidth,
            .right = RECT_WIDTH(&win->rect) - windowTheme.edgeWidth - windowTheme.paddingWidth,
            .bottom = windowTheme.topbarHeight + windowTheme.edgeWidth - windowTheme.paddingWidth,
        };
        window_draw_edge(win, &topBar, windowTheme.edgeWidth, windowTheme.dark, windowTheme.highlight);
        RECT_SHRINK(&topBar, windowTheme.edgeWidth);
        if (win->selected)
        {
            window_draw_gradient(win, &topBar, windowTheme.selected, windowTheme.selectedHighlight, GRADIENT_HORIZONTAL, false);
        }
        else
        {
            window_draw_gradient(win, &topBar, windowTheme.unSelected, windowTheme.unSelectedHighlight, GRADIENT_HORIZONTAL, false);
        }
    }
    break;
    }

    return 0;
}

static window_t* window_new_raw(display_t* disp, const char* name, const rect_t* rect, surface_type_t type, procedure_t procedure)
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

    surface_id_t id = display_gen_id(disp);

    cmd_t cmd;
    cmd.type = CMD_SURFACE_NEW;
    cmd.surfaceNew.id = id;
    cmd.surfaceNew.type = type;
    cmd.surfaceNew.rect = *rect;
    display_cmds_push(disp, &cmd);
    display_cmds_flush(disp);

    list_entry_init(&win->entry);
    win->id = id;
    strcpy(win->name, name);
    win->rect = *rect;
    win->type = type;
    win->proc = procedure;
    win->display = disp;
    win->selected = false;
    list_push(&disp->windows, &win->entry);
    return win;
}

window_t* window_new(display_t* disp, const char* name, const rect_t* rect, surface_type_t type, window_flags_t flags, procedure_t procedure)
{
    /*if (flags & WINDOW_DECO)
    {
        rect_t decoRect =
        {
            .left = rect->left - windowTheme.edgeWidth,
            .top = rect->top - windowTheme.edgeWidth - windowTheme.topbarHeight - windowTheme.paddingWidth,
            .right = rect->right + windowTheme.edgeWidth,
            .bottom = rect->bottom + windowTheme.edgeWidth,
        };
        window_t* deco = window_new_raw(disp, parent, "deco", &decoRect, display_gen_id(disp), type, window_deco_procedure);
        if (deco == NULL)
        {
            return NULL;
        }
        rect_t newRect =
        {
            .left = windowTheme.edgeWidth,
            .top = windowTheme.edgeWidth + windowTheme.topbarHeight + windowTheme.paddingWidth,
            .right = windowTheme.edgeWidth + RECT_WIDTH(rect),
            .bottom = windowTheme.edgeWidth + windowTheme.topbarHeight + windowTheme.paddingWidth + RECT_HEIGHT(rect),
        };
        window_t* win = window_new_raw(disp, deco, name, &newRect, id, type, procedure);
        if (win == NULL)
        {
            window_free(deco);
            return NULL;
        }
        win->deco = deco;
        return win;
    }
    else*/
    {
        return window_new_raw(disp, name, rect, type, procedure);
    }
}

void window_free(window_t* win)
{
    // Send fake free event.
    event_t event = {.type = LEVENT_FREE, .target = win->id};
    win->proc(win, &event);

    cmd_t cmd = {.type = CMD_SURFACE_FREE, .surfaceFree.target = win->id};
    display_cmds_push(win->display, &cmd);
    display_cmds_flush(win->display);
    list_remove(&win->entry);
    free(win);
}

void window_get_rect(window_t* win, rect_t* rect)
{
    *rect = win->rect;
}

void window_get_local_rect(window_t* win, rect_t* rect)
{
    *rect = RECT_INIT_DIM(0, 0, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
}

void window_draw_rect(window_t* win, const rect_t* rect, pixel_t pixel)
{
    cmd_t cmd = {.type = CMD_DRAW_RECT, .drawRect = {.target = win->id, .rect = *rect, .pixel = pixel}};
    display_cmds_push(win->display, &cmd);
}

void window_draw_edge(window_t* win, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    cmd_t cmd = {.type = CMD_DRAW_EDGE, .drawEdge = {.target = win->id, .rect = *rect, .width = width, .foreground = foreground, .background = background}};
    display_cmds_push(win->display, &cmd);
}

void window_draw_gradient(window_t* win, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type, bool addNoise)
{
    cmd_t cmd = {.type = CMD_DRAW_GRADIENT, .drawGradient = {.target = win->id, .rect = *rect, .start = start, .end = end, .type = type, .addNoise = addNoise}};
    display_cmds_push(win->display, &cmd);
}
