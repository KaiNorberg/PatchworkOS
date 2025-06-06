#define __STDC_WANT_LIB_EXT1__ 1
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_DECO_CLOSE_BUTTON_INDEX 0
#define WINDOW_DECO_BUTTON_AMOUNT 1

#define WINDOW_DECO_CLOSE_BUTTON_ID (WINDOW_DECO_ELEM_ID - (1 + WINDOW_DECO_CLOSE_BUTTON_INDEX))

typedef struct
{
    bool isFocused;
    bool isDragging;
    point_t dragOffset;
    image_t* closeIcon;
} deco_private_t;

static void window_deco_titlebar_rect(window_t* win, element_t* elem, rect_t* rect)
{
    rect_t contentRect = element_get_content_rect(elem);

    int64_t frameWidth = element_get_int(elem, INT_FRAME_SIZE);
    int64_t titlebarHeight = element_get_int(elem, INT_TITLEBAR_SIZE);
    int64_t smallPadding = element_get_int(elem, INT_SMALL_PADDING);

    *rect = (rect_t){
        .left = frameWidth + smallPadding,
        .top = frameWidth + smallPadding,
        .right = RECT_WIDTH(&contentRect) - frameWidth - smallPadding,
        .bottom = frameWidth + titlebarHeight,
    };
}

static void window_deco_button_rect(window_t* win, element_t* elem, rect_t* rect, uint64_t index)
{
    window_deco_titlebar_rect(win, elem, rect);
    RECT_SHRINK(rect, element_get_int(elem, INT_FRAME_SIZE));
    uint64_t size = (rect->bottom - rect->top);
    rect->left = rect->right - size * (index + 1);
}

static void window_deco_draw_titlebar(window_t* win, element_t* elem, drawable_t* draw)
{
    deco_private_t* private = element_get_private(elem);

    rect_t titlebar;
    window_deco_titlebar_rect(win, elem, &titlebar);

    int64_t panelSize = element_get_int(elem, INT_BIG_PADDING);
    int64_t bigPadding = element_get_int(elem, INT_BIG_PADDING);
    int64_t frameWidth = element_get_int(elem, INT_FRAME_SIZE);
    pixel_t highlight = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_HIGHLIGHT);
    pixel_t shadow = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_SHADOW);
    pixel_t selectedStart = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_SELECTED_START);
    pixel_t selectedEnd = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_SELECTED_END);
    pixel_t unselectedStart = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_UNSELECTED_START);
    pixel_t unselectedEnd = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_UNSELECTED_END);
    pixel_t foreground = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_FOREGROUND_NORMAL);

    draw_frame(draw, &titlebar, frameWidth, shadow, highlight);
    RECT_SHRINK(&titlebar, frameWidth);
    if (private->isFocused)
    {
        draw_gradient(draw, &titlebar, selectedStart, selectedEnd, DIRECTION_HORIZONTAL, false);
    }
    else
    {
        draw_gradient(draw, &titlebar, unselectedStart, unselectedEnd, DIRECTION_HORIZONTAL, false);
    }

    // Make some space so the text does not touch the buttons or the edge of the titlebar
    titlebar.left += bigPadding;
    titlebar.right -= panelSize;
    draw_text(draw, &titlebar, NULL, ALIGN_MIN, ALIGN_CENTER, foreground, win->name);
}

static void window_deco_handle_dragging(window_t* win, element_t* elem, const event_mouse_t* event)
{
    deco_private_t* private = element_get_private(elem);

    rect_t titlebarWithoutButtons;
    window_deco_titlebar_rect(win, elem, &titlebarWithoutButtons);
    if (!(win->flags & WINDOW_NO_CONTROLS))
    {
        rect_t lastButton;
        window_deco_button_rect(win, elem, &lastButton, WINDOW_DECO_BUTTON_AMOUNT - 1);
        titlebarWithoutButtons.right = lastButton.left;
    }

    if (private->isDragging)
    {
        rect_t rect = RECT_INIT_DIM(event->screenPos.x - private->dragOffset.x,
            event->screenPos.y - private->dragOffset.y, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
        window_move(win, &rect);

        if (!(event->held & MOUSE_LEFT))
        {
            private->isDragging = false;
        }
    }
    else if (RECT_CONTAINS_POINT(&titlebarWithoutButtons, &event->pos) && event->pressed & MOUSE_LEFT)
    {
        private->dragOffset =
            (point_t){.x = event->screenPos.x - win->rect.left, .y = event->screenPos.y - win->rect.top};
        private->isDragging = true;
    }
}

static uint64_t window_deco_procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case LEVENT_INIT:
    {
        deco_private_t* private = malloc(sizeof(deco_private_t));
        if (private == NULL)
        {
            return ERR;
        }

        private->isFocused = false;
        private->isDragging = false;
        element_set_private(elem, private);

        if (win->flags & WINDOW_NO_CONTROLS)
        {
            break;
        }

        rect_t closeRect;
        window_deco_button_rect(win, elem, &closeRect, WINDOW_DECO_CLOSE_BUTTON_INDEX);
        element_t* closeButton = button_new(elem, WINDOW_DECO_CLOSE_BUTTON_ID, &closeRect, "", ELEMENT_NO_OUTLINE);
        if (closeButton == NULL)
        {
            return ERR;
        }

        // Note: element_set_image failes safely here
        private->closeIcon = image_new(window_get_display(win), element_get_string(elem, STRING_ICON_CLOSE));
        element_set_image(closeButton, private->closeIcon);
    }
    break;
    case LEVENT_FREE:
    {
        deco_private_t* private = element_get_private(elem);
        if (private != NULL)
        {
            if (private->closeIcon != NULL)
            {
                image_free(private->closeIcon);
            }
            free(private);
        }
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect = element_get_content_rect(elem);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        pixel_t background = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_NORMAL);
        pixel_t highlight = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_HIGHLIGHT);
        pixel_t shadow = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_SHADOW);
        int64_t frameSize = element_get_int(elem, INT_FRAME_SIZE);

        draw_frame(&draw, &rect, frameSize, highlight, shadow);
        RECT_SHRINK(&rect, frameSize);
        draw_rect(&draw, &rect, background);
        window_deco_draw_titlebar(win, elem, &draw);

        element_draw_end(elem, &draw);
    }
    break;
    case LEVENT_ACTION:
    {
        if (event->lAction.type != ACTION_RELEASE)
        {
            break;
        }

        switch (event->lAction.source)
        {
        case WINDOW_DECO_CLOSE_BUTTON_ID:
        {
            display_events_push(win->disp, win->surface, LEVENT_QUIT, NULL, 0);
        }
        break;
        }
    }
    break;
    case EVENT_REPORT:
    {
        if (!(event->report.flags & REPORT_IS_FOCUSED))
        {
            break;
        }

        deco_private_t* private = element_get_private(elem);
        private->isFocused = event->report.info.isFocused;

        drawable_t draw;
        element_draw_begin(elem, &draw);

        window_deco_draw_titlebar(win, elem, &draw);

        element_draw_end(elem, &draw);
    }
    break;
    case EVENT_MOUSE:
    {
        window_deco_handle_dragging(win, elem, &event->mouse);
    }
    break;
    }

    return 0;
}

window_t* window_new(display_t* disp, const char* name, const rect_t* rect, surface_type_t type, window_flags_t flags,
    procedure_t procedure, void* private)
{
    if (strnlen_s(name, MAX_NAME + 1) >= MAX_NAME)
    {
        return NULL;
    }

    window_t* win = malloc(sizeof(window_t));
    if (win == NULL)
    {
        return NULL;
    }

    list_entry_init(&win->entry);
    win->disp = disp;
    strcpy(win->name, name);
    win->rect = *rect;
    win->invalidRect = (rect_t){0};
    win->type = type;
    win->flags = flags;
    win->buffer = NULL;
    win->root = NULL;
    win->clientElement = NULL;

    int64_t frameWidth = theme_get_int(INT_FRAME_SIZE, NULL);
    int64_t titlebarSize = theme_get_int(INT_TITLEBAR_SIZE, NULL);

    if (flags & WINDOW_DECO)
    {
        // Expand window to fit decorations
        win->rect.left -= frameWidth;
        win->rect.top -= frameWidth + titlebarSize;
        win->rect.right += frameWidth;
        win->rect.bottom += frameWidth;
    }

    cmd_surface_new_t* cmd = display_cmds_push(disp, CMD_SURFACE_NEW, sizeof(cmd_surface_new_t));
    cmd->type = win->type;
    cmd->rect = win->rect;
    strcpy(cmd->name, win->name);
    display_cmds_flush(disp);

    event_t event;
    display_wait_for_event(disp, &event, EVENT_SURFACE_NEW);
    strcpy(win->shmem, event.surfaceNew.shmem);
    win->surface = event.target;

    fd_t shmem = openf("sys:/shmem/%s", win->shmem);
    if (shmem == ERR)
    {
        window_free(win);
        return NULL;
    }
    win->buffer =
        mmap(shmem, NULL, RECT_WIDTH(&win->rect) * RECT_HEIGHT(&win->rect) * sizeof(pixel_t), PROT_READ | PROT_WRITE);
    close(shmem);

    if (win->buffer == NULL)
    {
        window_free(win);
        return NULL;
    }

    if (flags & WINDOW_DECO)
    {
        rect_t decoRect = RECT_INIT_DIM(0, 0, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
        win->root =
            element_new_root(win, WINDOW_DECO_ELEM_ID, &decoRect, "deco", ELEMENT_NONE, window_deco_procedure, NULL);
        if (win->root == NULL)
        {
            window_free(win);
            return NULL;
        }

        // Client area should not contain the decorations
        rect_t clientRect = {
            .left = frameWidth,
            .top = frameWidth + titlebarSize,
            .right = frameWidth + RECT_WIDTH(rect),
            .bottom = frameWidth + titlebarSize + RECT_HEIGHT(rect),
        };
        win->clientElement =
            element_new(win->root, WINDOW_CLIENT_ELEM_ID, &clientRect, "client", ELEMENT_NONE, procedure, private);
        if (win->clientElement == NULL)
        {
            window_free(win);
            return NULL;
        }
    }
    else
    {
        rect_t rootElemRect = RECT_INIT_DIM(0, 0, RECT_WIDTH(rect), RECT_HEIGHT(rect));
        win->root =
            element_new_root(win, WINDOW_CLIENT_ELEM_ID, &rootElemRect, "client", ELEMENT_NONE, procedure, private);
        if (win->root == NULL)
        {
            window_free(win);
            return NULL;
        }
        win->clientElement = win->root;
    }

    list_push(&disp->windows, &win->entry);
    return win;
}

void window_free(window_t* win)
{
    if (win->root != NULL)
    {
        element_free(win->root);
    }

    cmd_surface_free_t* cmd = display_cmds_push(win->disp, CMD_SURFACE_FREE, sizeof(cmd_surface_free_t));
    cmd->target = win->surface;
    display_cmds_flush(win->disp);

    if (win->buffer != NULL)
    {
        munmap(win->buffer, RECT_WIDTH(&win->rect) * RECT_HEIGHT(&win->rect) * sizeof(pixel_t));
    }

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

display_t* window_get_display(window_t* win)
{
    return win->disp;
}

surface_id_t window_get_id(window_t* win)
{
    return win->surface;
}

surface_type_t window_get_type(window_t* win)
{
    return win->type;
}

element_t* window_get_client_element(window_t* win)
{
    return win->clientElement;
}

uint64_t window_move(window_t* win, const rect_t* rect)
{
    bool isSizeChanged = RECT_WIDTH(&win->rect) != RECT_WIDTH(rect) || RECT_HEIGHT(&win->rect) != RECT_HEIGHT(rect);

    if (isSizeChanged && !(win->flags & WINDOW_RESIZABLE))
    {
        return ERR;
    }

    cmd_surface_move_t* cmd = display_cmds_push(win->disp, CMD_SURFACE_MOVE, sizeof(cmd_surface_move_t));
    cmd->target = win->surface;
    cmd->rect = *rect;
    display_cmds_flush(win->disp);

    return 0;
}

uint64_t window_set_timer(window_t* win, timer_flags_t flags, clock_t timeout)
{
    cmd_surface_timer_set_t* cmd = display_cmds_push(win->disp, CMD_SURFACE_TIMER_SET, sizeof(cmd_surface_timer_set_t));
    cmd->target = win->surface;
    cmd->flags = flags;
    cmd->timeout = timeout;
    display_cmds_flush(win->disp);

    return 0;
}

void window_invalidate(window_t* win, const rect_t* rect)
{
    if (RECT_AREA(&win->invalidRect) == 0)
    {
        win->invalidRect = *rect;
    }
    else
    {
        RECT_EXPAND_TO_CONTAIN(&win->invalidRect, rect);
    }
}

void window_invalidate_flush(window_t* win)
{
    if (RECT_AREA(&win->invalidRect) != 0)
    {
        cmd_surface_invalidate_t* cmd =
            display_cmds_push(win->disp, CMD_SURFACE_INVALIDATE, sizeof(cmd_surface_invalidate_t));
        cmd->target = win->surface;
        cmd->invalidRect = win->invalidRect;

        win->invalidRect = (rect_t){0};
    }
}

uint64_t window_dispatch(window_t* win, const event_t* event)
{
    switch (event->type)
    {
    case LEVENT_INIT:
    {
        element_t* elem = element_find(win->root, event->lInit.id);
        if (elem == NULL)
        {
            return ERR;
        }

        if (element_dispatch(elem, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    case LEVENT_REDRAW:
    {
        element_t* elem = element_find(win->root, event->lRedraw.id);
        if (elem == NULL)
        {
            return ERR;
        }

        if (element_dispatch(elem, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    case LEVENT_FORCE_ACTION:
    {
        element_t* elem = element_find(win->root, event->lForceAction.dest);
        if (elem == NULL)
        {
            return ERR;
        }

        if (element_dispatch(elem, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    case EVENT_REPORT:
    {
        if (event->report.flags & REPORT_RECT)
        {
            rect_t newRect = event->report.info.rect;

            if (RECT_WIDTH(&win->rect) != RECT_WIDTH(&newRect) || RECT_HEIGHT(&win->rect) != RECT_HEIGHT(&newRect))
            {
                levent_redraw_t event;
                event.id = win->root->id;
                event.shouldPropagate = true;
                display_events_push(win->disp, win->surface, LEVENT_REDRAW, &event, sizeof(levent_redraw_t));
            }

            win->rect = newRect;
        }

        if (element_dispatch(win->root, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    default:
    {
        if (element_dispatch(win->root, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    }

    window_invalidate_flush(win);

    return 0;
}

void window_set_focus(window_t* win)
{
    cmd_surface_focus_set_t* cmd = display_cmds_push(win->disp, CMD_SURFACE_FOCUS_SET, sizeof(cmd_surface_focus_set_t));
    cmd->isGlobal = false;
    cmd->target = win->surface;
    display_cmds_flush(win->disp);
}

void window_set_is_visible(window_t* win, bool isVisible)
{
    cmd_surface_visible_set_t* cmd =
        display_cmds_push(win->disp, CMD_SURFACE_VISIBLE_SET, sizeof(cmd_surface_visible_set_t));
    cmd->isGlobal = false;
    cmd->target = win->surface;
    cmd->isVisible = isVisible;
    display_cmds_flush(win->disp);
}
