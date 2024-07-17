#include "win_background.h"
#include "win_internal.h"

#include <sys/win.h>
#include <sys/gfx.h>
#include <sys/dwm.h>
#include <sys/mouse.h>

#ifndef __EMBED__

static void win_draw_topbar(win_t* window, surface_t* surface)
{
    rect_t localArea = RECT_INIT_SURFACE(surface);

    rect_t topBar = (rect_t){
        .left = localArea.left + theme.edgeWidth + TOPBAR_PADDING,
        .top = localArea.top + theme.edgeWidth + TOPBAR_PADDING,
        .right = localArea.right - theme.edgeWidth - TOPBAR_PADDING,
        .bottom = localArea.top + TOPBAR_HEIGHT + theme.edgeWidth - TOPBAR_PADDING,
    };
    gfx_rect(surface, &topBar, window->selected ? theme.selected : theme.unSelected);
    gfx_edge(surface, &topBar, theme.edgeWidth, theme.shadow, theme.highlight);
}

static void win_draw_border_and_background(win_t* window, surface_t* surface)
{
    if (window->type == DWM_WINDOW)
    {
        rect_t localArea = RECT_INIT_SURFACE(surface);

        gfx_rect(surface, &localArea, theme.background);
        gfx_edge(surface, &localArea, theme.edgeWidth, theme.highlight, theme.shadow);
    }
}

static void win_handle_drag(win_t* window, const msg_mouse_t* data)
{
    rect_t topBar = (rect_t){
        .left = window->pos.x + theme.edgeWidth,
        .top = window->pos.y + theme.edgeWidth,
        .right = window->pos.x + window->width - theme.edgeWidth,
        .bottom = window->pos.y + TOPBAR_HEIGHT + theme.edgeWidth,
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

void win_expand_to_window(rect_t* clientArea, dwm_type_t type)
{
    if (type == DWM_WINDOW)
    {
        clientArea->left -= theme.edgeWidth;
        clientArea->top -= theme.edgeWidth + TOPBAR_HEIGHT;
        clientArea->right += theme.edgeWidth;
        clientArea->bottom += theme.edgeWidth;
    }
}

void win_shrink_to_client(rect_t* windowArea, dwm_type_t type)
{
    if (type == DWM_WINDOW)
    {
        windowArea->left += theme.edgeWidth;
        windowArea->top += theme.edgeWidth + TOPBAR_HEIGHT;
        windowArea->right -= theme.edgeWidth;
        windowArea->bottom -= theme.edgeWidth;
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
