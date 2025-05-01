#include "internal.h"

#include <stdlib.h>

element_t* element_new(element_t* parent, const rect_t* rect, procedure_t procedure, void* private)
{
    element_t* elem = malloc(sizeof(element_t));
    if (elem == NULL)
    {
        return NULL;
    }
    list_entry_init(&elem->entry);
    list_init(&elem->children);
    elem->rect = *rect;
    elem->proc = procedure;
    elem->private = private;

    if (parent != NULL)
    {
        elem->win = parent->win;
        elem->parent = parent;
        list_push(&parent->children, &elem->entry);
    }
    else
    {
        elem->win = NULL;
        elem->parent = NULL;
    }

    return elem;
}

static void element_free_children(element_t* elem)
{
    element_t* child;
    element_t* temp;
    LIST_FOR_EACH_SAFE(child, temp, &elem->children, entry)
    {
        list_remove(&child->entry);
        element_free(child);
    }
}

void element_free(element_t* elem)
{
    // Send fake free event
    event_t event = {.target = elem->win->id, .type = LEVENT_FREE};
    elem->proc(elem->win, elem, &event);

    element_free_children(elem);
    free(elem);
}

void element_private_set(element_t* elem, void* private)
{
    elem->private = private;
}

void* element_private(element_t* elem)
{
    return elem->private;
}

void element_rect(element_t* elem, rect_t* rect)
{
    *rect = elem->rect;
}

void element_content_rect(element_t* elem, rect_t* rect)
{
    *rect = RECT_INIT_DIM(0, 0, RECT_WIDTH(&elem->rect), RECT_HEIGHT(&elem->rect));
}

void element_global_rect(element_t* elem, rect_t* rect)
{
    point_t offset = {.x = elem->rect.left, .y = elem->rect.top};
    element_t* parent = elem->parent;
    while (parent != NULL)
    {
        offset.x += parent->rect.left;
        offset.y += parent->rect.top;
        parent = parent->parent;
    }

    *rect = RECT_INIT_DIM(offset.x, offset.y, RECT_WIDTH(&elem->rect), RECT_HEIGHT(&elem->rect));
}

void element_rect_to_global(element_t* elem, rect_t* dest, const rect_t* src)
{
    rect_t globalRect;
    element_global_rect(elem, &globalRect);
    *dest = (rect_t){
        .left = globalRect.left + src->left,
        .top = globalRect.top + src->top,
        .right = globalRect.left + src->right,
        .bottom = globalRect.top + src->bottom,
    };
}

void element_draw_rect(element_t* elem, const rect_t* rect, pixel_t pixel)
{
    cmd_draw_rect_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_RECT, cmd_draw_rect_t);
    cmd.target = elem->win->id;
    element_rect_to_global(elem, &cmd.rect, rect);
    cmd.pixel = pixel;

    display_cmds_push(elem->win->disp, &cmd.header);
}

void element_draw_edge(element_t* elem, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    cmd_draw_edge_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_EDGE, cmd_draw_edge_t);
    cmd.target = elem->win->id;
    element_rect_to_global(elem, &cmd.rect, rect);
    cmd.width = width;
    cmd.foreground = foreground;
    cmd.background = background;

    display_cmds_push(elem->win->disp, &cmd.header);
}

void element_draw_gradient(element_t* elem, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type, bool addNoise)
{
    cmd_draw_gradient_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_GRADIENT, cmd_draw_gradient_t);
    cmd.target = elem->win->id;
    element_rect_to_global(elem, &cmd.rect, rect);
    cmd.start = start;
    cmd.end = end;
    cmd.type = type;
    cmd.addNoise = addNoise;

    display_cmds_push(elem->win->disp, &cmd.header);
}

uint64_t element_dispatch(element_t* elem, event_t* event, bool propagate)
{
    if (elem->proc(elem->win, elem, event) == ERR)
    {
        return ERR;
    }

    if (propagate)
    {
        element_t* child;
        LIST_FOR_EACH(child, &elem->children, entry)
        {
            if (element_dispatch(child, event, propagate) == ERR)
            {
                return ERR;
            }
        }
    }

    return 0;
}
