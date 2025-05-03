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

typedef struct
{
    bool focused;
} deco_private_t;

static void window_deco_draw_topbar(window_t* win, element_t* elem)
{
    deco_private_t* private = element_private(elem);

    rect_t rect;
    element_content_rect(elem, &rect);

    rect_t topBar = (rect_t){
        .left = windowTheme.edgeWidth + windowTheme.paddingWidth,
        .top = windowTheme.edgeWidth + windowTheme.paddingWidth,
        .right = RECT_WIDTH(&rect) - windowTheme.edgeWidth - windowTheme.paddingWidth,
        .bottom = windowTheme.topbarHeight + windowTheme.edgeWidth - windowTheme.paddingWidth,
    };
    element_draw_edge(elem, &topBar, windowTheme.edgeWidth, windowTheme.dark, windowTheme.highlight);
    RECT_SHRINK(&topBar, windowTheme.edgeWidth);
    if (private->focused)
    {
        element_draw_gradient(elem, &topBar, windowTheme.selected, windowTheme.selectedHighlight, GRADIENT_HORIZONTAL, false);
    }
    else
    {
        element_draw_gradient(elem, &topBar, windowTheme.unSelected, windowTheme.unSelectedHighlight, GRADIENT_HORIZONTAL, false);
    }

    topBar.left += windowTheme.paddingWidth * 3;
    topBar.right -= windowTheme.topbarHeight;
    element_draw_text(elem, &topBar, NULL, ALIGN_MIN, ALIGN_CENTER, windowTheme.background, 0, win->name);
}

static uint64_t window_deco_procedure(window_t* win, element_t* elem, const event_t* event)
{
    deco_private_t* private = element_private(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        private->focused = false;
    }
    break;
    case LEVENT_FREE:
    {
        free(private);
    }
    break;
    case EVENT_FOCUS_IN:
    {
        private->focused = true;
        window_deco_draw_topbar(win, elem);
    }
    break;
    case EVENT_FOCUS_OUT:
    {
        private->focused = false;
        window_deco_draw_topbar(win, elem);
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect(elem, &rect);
        element_draw_rect(elem, &rect, windowTheme.background);
        element_draw_edge(elem, &rect, windowTheme.edgeWidth, windowTheme.bright, windowTheme.dark);

        window_deco_draw_topbar(win, elem);
    }
    break;
    }

    return 0;
}

window_t* window_new(display_t* disp, const char* name, const rect_t* rect, surface_type_t type, window_flags_t flags,
    procedure_t procedure)
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
    win->id = display_gen_id(disp);
    strcpy(win->name, name);
    win->rect = *rect;
    win->type = type;

    if (flags & WINDOW_DECO)
    {
        win->rect.left -= windowTheme.edgeWidth;
        win->rect.top -= windowTheme.edgeWidth + windowTheme.topbarHeight + windowTheme.paddingWidth;
        win->rect.right += windowTheme.edgeWidth;
        win->rect.bottom += windowTheme.edgeWidth;

        deco_private_t* private = malloc(sizeof(deco_private_t));
        if (private == NULL)
        {
            free(win);
            return NULL;
        }

        rect_t decoRect = RECT_INIT_DIM(0, 0, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
        win->root = element_new_root(win, WINDOW_DECO_ELEM_ID, &decoRect, window_deco_procedure, private);
        if (win->root == NULL)
        {
            free(private);
            free(win);
            return NULL;
        }

        rect_t clientRect = {
            .left = windowTheme.edgeWidth,
            .top = windowTheme.edgeWidth + windowTheme.topbarHeight + windowTheme.paddingWidth,
            .right = windowTheme.edgeWidth + RECT_WIDTH(rect),
            .bottom = windowTheme.edgeWidth + windowTheme.topbarHeight + windowTheme.paddingWidth + RECT_HEIGHT(rect),
        };
        if (element_new(win->root, WINDOW_CLIENT_ELEM_ID, &clientRect, procedure, NULL) == NULL)
        {
            element_free(win->root);
            free(private);
            free(win);
        }
    }
    else
    {
        rect_t rootElemRect = RECT_INIT_DIM(0, 0, RECT_WIDTH(rect), RECT_HEIGHT(rect));
        win->root = element_new_root(win, WINDOW_CLIENT_ELEM_ID, &rootElemRect, procedure, NULL);
        if (win->root == NULL)
        {
            free(win);
            return NULL;
        }
    }

    cmd_surface_new_t cmd;
    CMD_INIT(&cmd, CMD_SURFACE_NEW, cmd_surface_new_t);
    cmd.id = win->id;
    cmd.type = win->type;
    cmd.rect = win->rect;
    display_cmds_push(disp, &cmd.header);
    display_cmds_flush(disp);

    list_push(&disp->windows, &win->entry);
    return win;
}

void window_free(window_t* win)
{
    element_free(win->root);

    cmd_surface_free_t cmd;
    CMD_INIT(&cmd, CMD_SURFACE_FREE, cmd_surface_free_t);
    cmd.target = win->id;
    display_cmds_push(win->disp, &cmd.header);
    display_cmds_flush(win->disp);

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
    default:
    {
        if (element_dispatch(win->root, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    }

    return 0;
}
