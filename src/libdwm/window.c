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

static uint64_t window_deco_procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case EVENT_INIT:
    {

    }
    break;
    case EVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect(elem, &rect);
        element_draw_rect(elem, &rect, windowTheme.background);
        element_draw_edge(elem, &rect, windowTheme.edgeWidth, windowTheme.bright, windowTheme.dark);

        rect_t topBar = (rect_t){
            .left = windowTheme.edgeWidth + windowTheme.paddingWidth,
            .top = windowTheme.edgeWidth + windowTheme.paddingWidth,
            .right = RECT_WIDTH(&rect) - windowTheme.edgeWidth - windowTheme.paddingWidth,
            .bottom = windowTheme.topbarHeight + windowTheme.edgeWidth - windowTheme.paddingWidth,
        };
        element_draw_edge(elem, &topBar, windowTheme.edgeWidth, windowTheme.dark, windowTheme.highlight);
        RECT_SHRINK(&topBar, windowTheme.edgeWidth);
        if (win->selected)
        {
            element_draw_gradient(elem, &topBar, windowTheme.selected, windowTheme.selectedHighlight, GRADIENT_HORIZONTAL, false);
        }
        else
        {
            element_draw_gradient(elem, &topBar, windowTheme.unSelected, windowTheme.unSelectedHighlight, GRADIENT_HORIZONTAL,
                false);
        }
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
    win->id = display_gen_id(disp);
    strcpy(win->name, name);
    win->rect = *rect;
    win->type = type;
    win->disp = disp;
    win->selected = false;

    if (flags & WINDOW_DECO)
    {
        win->rect.left -= windowTheme.edgeWidth;
        win->rect.top -= windowTheme.edgeWidth + windowTheme.topbarHeight + windowTheme.paddingWidth;
        win->rect.right += windowTheme.edgeWidth;
        win->rect.bottom += windowTheme.edgeWidth;

        rect_t decoRect = RECT_INIT_DIM(0, 0, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
        win->root = element_new(NULL, &decoRect, window_deco_procedure);
        if (win->root == NULL)
        {
            free(win);
            return NULL;
        }
        win->root->win = win;

        rect_t clientRect = {
            .left = windowTheme.edgeWidth,
            .top = windowTheme.edgeWidth + windowTheme.topbarHeight + windowTheme.paddingWidth,
            .right = windowTheme.edgeWidth + RECT_WIDTH(rect),
            .bottom = windowTheme.edgeWidth + windowTheme.topbarHeight + windowTheme.paddingWidth + RECT_HEIGHT(rect),
        };
        if (element_new(win->root, &clientRect, procedure) == NULL)
        {
            element_free(win->root);
            free(win);
        }
    }
    else
    {
        rect_t rootElemRect = RECT_INIT_DIM(0, 0, RECT_WIDTH(rect), RECT_HEIGHT(rect));
        win->root = element_new(NULL, &rootElemRect, procedure);
        if (win->root == NULL)
        {
            free(win);
            return NULL;
        }
        win->root->win = win;
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

static uint64_t window_element_dispatch_event(window_t* win, element_t* elem, event_t* event, bool propagate)
{
    if (elem->proc(win, elem, event) == ERR)
    {
        return ERR;
    }

    if (propagate)
    {
        element_t* child;
        LIST_FOR_EACH(child, &elem->children, entry)
        {
            if (window_element_dispatch_event(win, child, event, propagate) == ERR)
            {
                return ERR;
            }
        }
    }

    return 0;
}

uint64_t window_dispatch(window_t* win, event_t* event)
{
    switch (event->type)
    {
    default:
    {
        if (window_element_dispatch_event(win, win->root, event, true) == ERR)
        {
            return ERR;
        }
    }
    break;
    }

    return 0;
}
