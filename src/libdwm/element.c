#include "internal.h"

#include <stdlib.h>
#include <string.h>

static void element_send_init(element_t* elem)
{
    levent_init_t event;
    event.id = elem->id;
    display_events_push(elem->win->disp, elem->win->id, LEVENT_INIT, &event, sizeof(levent_init_t));
}

static element_t* element_new_raw(element_id_t id, const rect_t* rect, procedure_t procedure, void* private)
{
    element_t* elem = malloc(sizeof(element_t));
    if (elem == NULL)
    {
        return NULL;
    }
    list_entry_init(&elem->entry);
    list_init(&elem->children);
    elem->parent = NULL;
    elem->id = id;
    elem->rect = *rect;
    elem->proc = procedure;
    elem->win = NULL;
    elem->private = private;
    elem->invalidRect = (rect_t){0};
    return elem;
}

element_t* element_new(element_t* parent, element_id_t id, const rect_t* rect, procedure_t procedure, void* private)
{
    element_t* elem = element_new_raw(id, rect, procedure, private);
    if (elem == NULL)
    {
        return NULL;
    }

    elem->parent = parent;
    elem->win = parent->win;
    list_push(&parent->children, &elem->entry);

    element_send_init(elem);
    element_send_redraw(elem, false);
    return elem;
}

element_t* element_new_root(window_t* win, element_id_t id, const rect_t* rect, procedure_t procedure, void* private)
{
    element_t* elem = element_new_raw(id, rect, procedure, private);
    if (elem == NULL)
    {
        return NULL;
    }

    elem->win = win;

    element_send_init(elem);
    element_send_redraw(elem, false);
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

element_t* element_find(element_t* elem, element_id_t id)
{
    if (elem->id == id)
    {
        return elem;
    }

    element_t* child;
    LIST_FOR_EACH(child, &elem->children, entry)
    {
        if (child->id == id)
        {
            return child;
        }
        element_t* grandChild = element_find(child, id);
        if (grandChild != NULL)
        {
            return grandChild;
        }
    }

    return NULL;
}

void element_set_private(element_t* elem, void* private)
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
    point_t point;
    element_global_point(elem, &point);

    *rect = RECT_INIT_DIM(point.x, point.y, RECT_WIDTH(&elem->rect), RECT_HEIGHT(&elem->rect));
}

void element_global_point(element_t* elem, point_t* point)
{
    point_t offset = {.x = elem->rect.left, .y = elem->rect.top};
    element_t* parent = elem->parent;
    while (parent != NULL)
    {
        offset.x += parent->rect.left;
        offset.y += parent->rect.top;
        parent = parent->parent;
    }

    *point = offset;
}

void element_rect_to_global(element_t* elem, rect_t* dest, const rect_t* src)
{
    point_t point;
    element_global_point(elem, &point);
    *dest = (rect_t){
        .left = point.x + src->left,
        .top = point.y + src->top,
        .right = point.x + src->right,
        .bottom = point.y + src->bottom,
    };
}

void element_point_to_global(element_t* elem, point_t* dest, const point_t* src)
{
    point_t point;
    element_global_point(elem, &point);
    *dest = (point_t){
        .x = point.x + src->x,
        .y = point.y + src->y,
    };
}

void element_global_to_rect(element_t* elem, rect_t* dest, const rect_t* src)
{
    point_t point;
    element_global_point(elem, &point);
    *dest = (rect_t){
        .left = src->left - point.x,
        .top = src->top - point.y,
        .right = src->right - point.x,
        .bottom = src->bottom - point.y,
    };
}

void element_global_to_point(element_t* elem, point_t* dest, const point_t* src)
{
    point_t point;
    element_global_point(elem, &point);
    *dest = (point_t){
        .x = src->x - point.x,
        .y = src->y - point.y,
    };
}

void element_invalidate(element_t* elem, const rect_t* rect)
{
    if (RECT_AREA(&elem->invalidRect) == 0)
    {
        elem->invalidRect = *rect;
    }
    else
    {
        RECT_EXPAND_TO_CONTAIN(&elem->invalidRect, rect);
    }
}

void element_draw_rect(element_t* elem, const rect_t* rect, pixel_t pixel)
{
    cmd_draw_rect_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_RECT, sizeof(cmd));
    cmd.target = elem->win->id;
    element_rect_to_global(elem, &cmd.rect, rect);
    cmd.pixel = pixel;

    display_cmds_push(elem->win->disp, &cmd.header);

    element_invalidate(elem, rect);
}

void element_draw_edge(element_t* elem, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    cmd_draw_edge_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_EDGE, sizeof(cmd));
    cmd.target = elem->win->id;
    element_rect_to_global(elem, &cmd.rect, rect);
    cmd.width = width;
    cmd.foreground = foreground;
    cmd.background = background;

    display_cmds_push(elem->win->disp, &cmd.header);

    element_invalidate(elem, rect);
}

void element_draw_gradient(element_t* elem, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type,
    bool addNoise)
{
    cmd_draw_gradient_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_GRADIENT, sizeof(cmd));
    cmd.target = elem->win->id;
    element_rect_to_global(elem, &cmd.rect, rect);
    cmd.start = start;
    cmd.end = end;
    cmd.type = type;
    cmd.addNoise = addNoise;

    display_cmds_push(elem->win->disp, &cmd.header);

    element_invalidate(elem, rect);
}

void element_draw_string(element_t* elem, font_t* font, const point_t* point, pixel_t foreground, pixel_t background,
    const char* string, uint64_t length)
{
    uint8_t buffer[sizeof(cmd_draw_string_t) + MAX_PATH];

    cmd_draw_string_t* cmd;
    if (length >= MAX_PATH) // If string is small stack allocate memory
    {
        cmd = malloc(sizeof(cmd_draw_string_t) + length);
    }
    else
    {
        cmd = (void*)buffer;
    }

    if (font == NULL)
    {
        font = font_default(elem->win->disp);
    }

    CMD_INIT(cmd, CMD_DRAW_STRING, sizeof(cmd_draw_string_t));
    cmd->target = elem->win->id;
    cmd->fontId = font->id;
    element_point_to_global(elem, &cmd->point, point);
    cmd->foreground = foreground;
    cmd->background = background;
    cmd->length = length;
    memcpy(cmd->string, string, length);
    cmd->header.size += length;
    display_cmds_push(elem->win->disp, &cmd->header);

    if (length >= MAX_PATH)
    {
        free(cmd);
    }

    rect_t rect = RECT_INIT_DIM(point->x, point->y, font->width * length, font->height);
    element_invalidate(elem, &rect);
}

void element_draw_transfer(element_t* elem, const rect_t* destRect, const point_t* srcPoint)
{
    cmd_draw_transfer_t cmd;
    CMD_INIT(&cmd, CMD_DRAW_TRANSFER, sizeof(cmd));
    cmd.dest = elem->win->id;
    cmd.src = elem->win->id;
    element_rect_to_global(elem, &cmd.destRect, destRect);
    element_point_to_global(elem, &cmd.srcPoint, srcPoint);
    display_cmds_push(elem->win->disp, &cmd.header);

    element_invalidate(elem, destRect);
    rect_t srcRect = RECT_INIT_DIM(srcPoint->x, srcPoint->y, RECT_WIDTH(destRect), RECT_HEIGHT(destRect));
    element_invalidate(elem, &srcRect);
}

void element_draw_rim(element_t* elem, const rect_t* rect, uint64_t width, pixel_t pixel)
{
    rect_t leftRect = (rect_t){
        .left = rect->left,
        .top = rect->top + width - width / 2,
        .right = rect->left + width,
        .bottom = rect->bottom - width + width / 2,
    };
    element_draw_rect(elem, &leftRect, pixel);

    rect_t topRect = (rect_t){
        .left = rect->left + width - width / 2,
        .top = rect->top,
        .right = rect->right - width + width / 2,
        .bottom = rect->top + width,
    };
    element_draw_rect(elem, &topRect, pixel);

    rect_t rightRect = (rect_t){
        .left = rect->right - width,
        .top = rect->top + width - width / 2,
        .right = rect->right,
        .bottom = rect->bottom - width + width / 2,
    };
    element_draw_rect(elem, &rightRect, pixel);

    rect_t bottomRect = (rect_t){
        .left = rect->left + width - width / 2,
        .top = rect->bottom - width,
        .right = rect->right - width + width / 2,
        .bottom = rect->bottom,
    };
    element_draw_rect(elem, &bottomRect, pixel);
}

void element_draw_text(element_t* elem, const rect_t* rect, font_t* font, align_t xAlign, align_t yAlign,
    pixel_t foreground, pixel_t background, const char* text)
{
    if (text == NULL || *text == '\0')
    {
        return;
    }

    if (font == NULL)
    {
        font = font_default(elem->win->disp);
    }

    uint64_t maxWidth = RECT_WIDTH(rect);

    uint64_t textLen = strlen(text);
    uint64_t maxLen = maxWidth / font->width;

    uint64_t clampedLen = MIN(maxLen, textLen);
    uint64_t clampedWidth = clampedLen * font->width;

    point_t startPoint;
    switch (xAlign)
    {
    case ALIGN_CENTER:
    {
        startPoint.x = (rect->left + rect->right) / 2 - clampedWidth / 2;
    }
    break;
    case ALIGN_MAX:
    {
        startPoint.x = rect->right - clampedWidth;
    }
    break;
    case ALIGN_MIN:
    {
        startPoint.x = rect->left;
    }
    break;
    default:
    {
        return;
    }
    }

    switch (yAlign)
    {
    case ALIGN_CENTER:
    {
        startPoint.y = (rect->top + rect->bottom) / 2 - font->height / 2;
    }
    break;
    case ALIGN_MAX:
    {
        startPoint.y = MAX(rect->top, rect->bottom - (int64_t)font->height);
    }
    break;
    case ALIGN_MIN:
    {
        startPoint.y = rect->top;
    }
    break;
    default:
    {
        return;
    }
    }

    if (textLen > maxLen)
    {
        if (maxLen == 0)
        {
            return;
        }

        if (maxLen <= 3)
        {
            element_draw_string(elem, font, &startPoint, foreground, background, "...", maxLen);
            return;
        }

        element_draw_string(elem, font, &startPoint, foreground, background, text, maxLen - 3);
        startPoint.x += maxLen * font->width;
        element_draw_string(elem, font, &startPoint, foreground, background, "...", 3);
        return;
    }

    element_draw_string(elem, font, &startPoint, foreground, background, text, textLen);
}

void element_draw_ridge(element_t* elem, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    element_draw_edge(elem, rect, width / 2, background, foreground);

    rect_t innerRect = *rect;
    RECT_SHRINK(&innerRect, width / 2);
    element_draw_edge(elem, &innerRect, width / 2, foreground, background);
}

void element_send_redraw(element_t* elem, bool propagate)
{
    levent_redraw_t event;
    event.id = elem->id;
    event.propagate = propagate;
    display_events_push(elem->win->disp, elem->win->id, LEVENT_REDRAW, &event, sizeof(levent_redraw_t));
}

uint64_t element_dispatch(element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case LEVENT_INIT:
    case LEVENT_REDRAW:
    {
        if (elem->proc(elem->win, elem, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    case EVENT_MOUSE:
    {
        // Move mouse pos to be centered around elements origin.
        event_t movedEvent = *event;
        movedEvent.mouse.pos.x -= elem->rect.left;
        movedEvent.mouse.pos.y -= elem->rect.top;

        if (elem->proc(elem->win, elem, &movedEvent) == ERR)
        {
            return ERR;
        }

        element_t* child;
        LIST_FOR_EACH(child, &elem->children, entry)
        {
            if (element_dispatch(child, &movedEvent) == ERR)
            {
                return ERR;
            }
        }
    }
    break;
    default:
    {
        if (elem->proc(elem->win, elem, event) == ERR)
        {
            return ERR;
        }

        element_t* child;
        LIST_FOR_EACH(child, &elem->children, entry)
        {
            if (element_dispatch(child, event) == ERR)
            {
                return ERR;
            }
        }
    }
    break;
    }

    bool shouldPropegate = event->type == LEVENT_REDRAW && event->lRedraw.propagate;
    if (RECT_AREA(&elem->invalidRect) != 0 || shouldPropegate)
    {
        element_t* child;
        LIST_FOR_EACH(child, &elem->children, entry)
        {
            if (RECT_OVERLAP(&elem->invalidRect, &child->rect))
            {
                element_send_redraw(child, shouldPropegate);
            }
        }

        elem->invalidRect = (rect_t){0};
    }

    return 0;
}
