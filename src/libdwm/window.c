#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: this should be stored in some sort of config file, lua? make something custom?
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

#define WINDOW_DECO_CLOSE_BUTTON_INDEX 0
#define WINDOW_DECO_BUTTON_AMOUNT 1

#define WINDOW_DECO_CLOSE_BUTTON_ID (WINDOW_DECO_ELEM_ID - (1 + WINDOW_DECO_CLOSE_BUTTON_INDEX))

typedef struct
{
    bool focused;
    bool dragging;
    point_t dragOffset;
    font_t* bigFont;
} deco_private_t;

static void window_deco_topbar_rect(window_t* win, element_t* elem, rect_t* rect)
{
    rect_t contentRect;
    element_content_rect(elem, &contentRect);

    *rect = (rect_t){
        .left = windowTheme.edgeWidth + windowTheme.paddingWidth,
        .top = windowTheme.edgeWidth + windowTheme.paddingWidth,
        .right = RECT_WIDTH(&contentRect) - windowTheme.edgeWidth - windowTheme.paddingWidth,
        .bottom = windowTheme.topbarHeight + windowTheme.edgeWidth - windowTheme.paddingWidth,
    };
}

static void window_deco_button_rect(window_t* win, element_t* elem, rect_t* rect, uint64_t index)
{
    window_deco_topbar_rect(win, elem, rect);
    RECT_SHRINK(rect, windowTheme.edgeWidth);
    uint64_t size = (rect->bottom - rect->top);
    rect->left = rect->right - size * (index + 1);
}

static void window_deco_draw_topbar(window_t* win, element_t* elem, drawable_t* draw)
{
    deco_private_t* private = element_private(elem);

    rect_t topBar;
    window_deco_topbar_rect(win, elem, &topBar);

    draw_edge(draw, &topBar, windowTheme.edgeWidth, windowTheme.dark, windowTheme.highlight);
    RECT_SHRINK(&topBar, windowTheme.edgeWidth);
    if (private->focused)
    {
        draw_gradient(draw, &topBar, windowTheme.selected, windowTheme.selectedHighlight, GRADIENT_HORIZONTAL, false);
    }
    else
    {
        draw_gradient(draw, &topBar, windowTheme.unSelected, windowTheme.unSelectedHighlight, GRADIENT_HORIZONTAL,
            false);
    }

    topBar.left += windowTheme.paddingWidth * 3;
    topBar.right -= windowTheme.topbarHeight;
    draw_text(draw, &topBar, NULL, ALIGN_MIN, ALIGN_CENTER, windowTheme.background, 0, win->name);
}

static void window_deco_handle_dragging(window_t* win, element_t* elem, const event_mouse_t* event)
{
    deco_private_t* private = element_private(elem);

    rect_t topBarWithoutButtons;
    window_deco_topbar_rect(win, elem, &topBarWithoutButtons);
    if (!(win->flags & WINDOW_NO_CONTROLS))
    {
        rect_t lastButton;
        window_deco_button_rect(win, elem, &lastButton, WINDOW_DECO_BUTTON_AMOUNT - 1);
        topBarWithoutButtons.right = lastButton.right;
    }

    if (private->dragging)
    {
        rect_t rect = RECT_INIT_DIM(event->screenPos.x - private->dragOffset.x,
            event->screenPos.y - private->dragOffset.y, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
        window_move(win, &rect);

        if (!(event->held & MOUSE_LEFT))
        {
            private->dragging = false;
        }
    }
    else if (RECT_CONTAINS_POINT(&topBarWithoutButtons, &event->pos) && event->pressed & MOUSE_LEFT)
    {
        private->dragOffset =
            (point_t){.x = event->screenPos.x - win->rect.left, .y = event->screenPos.y - win->rect.top};
        private->dragging = true;
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

        private->focused = false;
        private->dragging = false;
        private->bigFont = font_new(window_display(win), DEFAULT_FONT, 32);
        element_set_private(elem, private);

        if (win->flags & WINDOW_NO_CONTROLS)
        {
            break;
        }

        rect_t closeButton;
        window_deco_button_rect(win, elem, &closeButton, WINDOW_DECO_CLOSE_BUTTON_INDEX);
        if (button_new(elem, WINDOW_DECO_CLOSE_BUTTON_ID, &closeButton, private->bigFont, windowTheme.dark,
                windowTheme.background, BUTTON_NONE, "x") == NULL)
        {
            return ERR;
        }
    }
    break;
    case LEVENT_FREE:
    {
        deco_private_t* private = element_private(elem);
        if (private != NULL)
        {
            font_free(private->bigFont);
            free(private);
        }
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect(elem, &rect);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        draw_rect(&draw, &rect, windowTheme.background);
        draw_edge(&draw, &rect, windowTheme.edgeWidth, windowTheme.bright, windowTheme.dark);

        window_deco_draw_topbar(win, elem, &draw);
    
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
    case EVENT_FOCUS_IN:
    {
        deco_private_t* private = element_private(elem);
        private->focused = true;

        drawable_t draw;
        element_draw_begin(elem, &draw);

        window_deco_draw_topbar(win, elem, &draw);
    
        element_draw_end(elem, &draw);
    }
    break;
    case EVENT_FOCUS_OUT:
    {
        deco_private_t* private = element_private(elem);
        private->focused = false;

        drawable_t draw;
        element_draw_begin(elem, &draw);

        window_deco_draw_topbar(win, elem, &draw);

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
    if (strlen(name) >= MAX_NAME)
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
    win->surface = display_gen_id(disp);
    strcpy(win->name, name);
    win->rect = *rect;
    win->invalidRect = (rect_t){0};
    win->type = type;
    win->flags = flags;

    if (flags & WINDOW_DECO)
    {
        win->rect.left -= windowTheme.edgeWidth;
        win->rect.top -= windowTheme.edgeWidth + windowTheme.topbarHeight;
        win->rect.right += windowTheme.edgeWidth;
        win->rect.bottom += windowTheme.edgeWidth;

        rect_t decoRect = RECT_INIT_DIM(0, 0, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
        win->root = element_new_root(win, WINDOW_DECO_ELEM_ID, &decoRect, window_deco_procedure, NULL);
        if (win->root == NULL)
        {
            free(win);
            return NULL;
        }

        rect_t clientRect = {
            .left = windowTheme.edgeWidth,
            .top = windowTheme.edgeWidth + windowTheme.topbarHeight,
            .right = windowTheme.edgeWidth + RECT_WIDTH(rect),
            .bottom = windowTheme.edgeWidth + windowTheme.topbarHeight + RECT_HEIGHT(rect),
        };
        win->clientElement = element_new(win->root, WINDOW_CLIENT_ELEM_ID, &clientRect, procedure, private);
        if (win->clientElement == NULL)
        {
            element_free(win->root);
            free(win);
            return NULL;
        }
    }
    else
    {
        rect_t rootElemRect = RECT_INIT_DIM(0, 0, RECT_WIDTH(rect), RECT_HEIGHT(rect));
        win->root = element_new_root(win, WINDOW_CLIENT_ELEM_ID, &rootElemRect, procedure, private);
        if (win->root == NULL)
        {
            free(win);
            return NULL;
        }
        win->clientElement = win->root;
    }

    cmd_surface_new_t* cmd = display_cmds_push(disp, CMD_SURFACE_NEW, sizeof(cmd_surface_new_t));
    cmd->id = win->surface;
    cmd->type = win->type;
    cmd->rect = win->rect;
    display_cmds_flush(disp);

    event_t event;
    display_wait_for_event(disp, &event, EVENT_SURFACE_NEW);

    strcpy(win->shmem, event.surfaceNew.shmem);
    win->buffer = NULL;

    fd_t shmem = openf("sys:/shmem/%s", win->shmem);
    if (shmem == ERR)
    {
        window_free(win);
        return NULL;
    }

    win->buffer = mmap(shmem, NULL, RECT_WIDTH(&win->rect) * RECT_HEIGHT(&win->rect) * sizeof(pixel_t), PROT_READ | PROT_WRITE);
    close(shmem);

    if (win->buffer == NULL)
    {
        window_free(win);
        return NULL;
    }

    list_push(&disp->windows, &win->entry);
    return win;
}

void window_free(window_t* win)
{
    element_free(win->root);

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

void window_rect(window_t* win, rect_t* rect)
{
    *rect = win->rect;
}

void window_local_rect(window_t* win, rect_t* rect)
{
    *rect = RECT_INIT_DIM(0, 0, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
}

display_t* window_display(window_t* win)
{
    return win->disp;
}

surface_id_t window_id(window_t* win)
{
    return win->surface;
}

surface_type_t window_type(window_t* win)
{
    return win->type;
}

element_t* window_client_element(window_t* win)
{
    return win->clientElement;
}

uint64_t window_move(window_t* win, const rect_t* rect)
{
    bool sizeChange = RECT_WIDTH(&win->rect) != RECT_WIDTH(rect) || RECT_HEIGHT(&win->rect) != RECT_HEIGHT(rect);

    if (sizeChange && !(win->flags & WINDOW_RESIZABLE))
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
    cmd_surface_set_timer_t* cmd = display_cmds_push(win->disp, CMD_SURFACE_SET_TIMER, sizeof(cmd_surface_set_timer_t));
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
        cmd_surface_invalidate_t* cmd = display_cmds_push(win->disp, CMD_SURFACE_INVALIDATE, sizeof(cmd_surface_invalidate_t));
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
    case EVENT_SURFACE_MOVE:
    {
        if (RECT_WIDTH(&win->rect) != RECT_WIDTH(&event->surfaceMove.rect) ||
            RECT_HEIGHT(&win->rect) != RECT_HEIGHT(&event->surfaceMove.rect))
        {
            levent_redraw_t event;
            event.id = win->root->id;
            event.propagate = true;
            display_events_push(win->disp, win->surface, LEVENT_REDRAW, &event, sizeof(levent_redraw_t));
        }

        win->rect = event->surfaceMove.rect;
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
