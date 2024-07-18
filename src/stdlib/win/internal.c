#ifndef __EMBED__

#include <sys/mouse.h>

#include "internal.h"

// TODO: this should be stored in some sort of config file, lua? make something custom?
win_theme_t theme = {
    .edgeWidth = 3,
    .rimWidth = 3,
    .ridgeWidth = 2,
    .highlight = 0xFFFCFCFC,
    .shadow = 0xFF6F6F6F,
    .bright = 0xFFFFFFFF,
    .dark = 0xFF000000,
    .background = 0xFFBFBFBF,
    .selected = 0xFF00007F,
    .unSelected = 0xFF7F7F7F,
    .topbarHeight = 40,
    .topbarPadding = 2,
};

static void win_draw_close_button(win_t* window, surface_t* surface, const rect_t* topbar)
{
    uint64_t width = RECT_HEIGHT(topbar);

    rect_t rect = {
        .left = topbar->right - width,
        .top = topbar->top,
        .right = topbar->right,
        .bottom = topbar->bottom,
    };
    gfx_rim(surface, &rect, theme.rimWidth, theme.dark);
    RECT_SHRINK(&rect, theme.rimWidth);

    gfx_edge(surface, &rect, theme.edgeWidth, theme.highlight, theme.shadow);
    RECT_SHRINK(&rect, theme.edgeWidth);
    gfx_rect(surface, &rect, theme.background);
}

static void win_draw_topbar(win_t* window, surface_t* surface)
{
    rect_t rect = {
        .left = theme.edgeWidth + theme.topbarPadding,
        .top = theme.edgeWidth + theme.topbarPadding,
        .right = surface->width - theme.edgeWidth - theme.topbarPadding,
        .bottom = theme.topbarHeight + theme.edgeWidth - theme.topbarPadding,
    };
    gfx_edge(surface, &rect, theme.edgeWidth, theme.dark, theme.highlight);
    RECT_SHRINK(&rect, theme.edgeWidth);
    gfx_rect(surface, &rect, window->selected ? theme.selected : theme.unSelected);

    win_draw_close_button(window, surface, &rect);
}

static void win_draw_border_and_background(win_t* window, surface_t* surface)
{
    if (window->type == DWM_WINDOW)
    {
        rect_t localArea = RECT_INIT_SURFACE(surface);

        gfx_rect(surface, &localArea, theme.background);
        gfx_edge(surface, &localArea, theme.edgeWidth, theme.bright, theme.dark);
    }
}

static void win_handle_drag(win_t* window, const msg_mouse_t* data)
{
    rect_t topBar = (rect_t){
        .left = window->pos.x + theme.edgeWidth,
        .top = window->pos.y + theme.edgeWidth,
        .right = window->pos.x + window->width - theme.edgeWidth,
        .bottom = window->pos.y + theme.topbarHeight + theme.edgeWidth,
    };

    if (window->moving)
    {
        rect_t rect = RECT_INIT_DIM(window->pos.x + data->deltaX, window->pos.y + data->deltaY, window->width, window->height);
        win_move(window, &rect);

        if (!(data->buttons & MOUSE_LEFT))
        {
            window->moving = false;
        }
    }
    else if (RECT_CONTAINS_POINT(&topBar, data->pos.x, data->pos.y) && data->buttons & MOUSE_LEFT)
    {
        window->moving = true;
    }
}

void win_background_procedure(win_t* window, const msg_t* msg)
{
    surface_t surface;
    win_window_surface(window, &surface);

    switch (msg->type)
    {
    case MSG_MOUSE:
    {
        msg_mouse_t* data = (msg_mouse_t*)msg->data;

        if (window->type == DWM_WINDOW)
        {
            win_handle_drag(window, data);
        }

        win_widget_send_all(window, WMSG_MOUSE, msg->data, sizeof(msg_mouse_t));
    }
    break;
    case MSG_SELECT:
    {
        window->selected = true;
        if (window->type == DWM_WINDOW)
        {
            win_draw_topbar(window, &surface);
        }
    }
    break;
    case MSG_DESELECT:
    {
        window->selected = false;
        if (window->type == DWM_WINDOW)
        {
            win_draw_topbar(window, &surface);
        }
    }
    break;
    case LMSG_REDRAW:
    {
        if (window->type == DWM_WINDOW)
        {
            win_draw_border_and_background(window, &surface);
            win_draw_topbar(window, &surface);
        }

        win_widget_send_all(window, WMSG_REDRAW, NULL, 0);
    }
    break;
    }

    if (RECT_AREA(&surface.invalidArea) != 0 &&
        flush(window->fd, window->buffer, window->width * window->height * sizeof(pixel_t), &surface.invalidArea) == ERR)
    {
        win_send(window, LMSG_QUIT, NULL, 0);
    }
}

#endif
