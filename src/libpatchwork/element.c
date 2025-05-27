#include "internal.h"

#include <stdlib.h>
#include <string.h>

static void element_send_init(element_t* elem)
{
    levent_init_t event;
    event.id = elem->id;
    display_events_push(elem->win->disp, elem->win->surface, LEVENT_INIT, &event, sizeof(levent_init_t));
}

static element_t* element_new_raw(element_id_t id, const rect_t* rect, const char* text, element_flags_t flags,
    procedure_t procedure, void* private)
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
    elem->proc = procedure;
    elem->win = NULL;
    elem->private = private;
    elem->rect = *rect;
    elem->flags = flags;
    elem->text = strdup(text);
    if (elem->text == NULL)
    {
        free(elem);
        return NULL;
    }
    elem->image = NULL;
    elem->imageProps = ELEMENT_IMAGE_PROPS_DEFAULT;
    elem->textProps = ELEMENT_TEXT_PROPS_DEFAULT;
    theme_override_init(&elem->theme);
    return elem;
}

element_t* element_new(element_t* parent, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags,
    procedure_t procedure, void* private)
{
    element_t* elem = element_new_raw(id, rect, text, flags, procedure, private);
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

element_t* element_new_root(window_t* win, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags,
    procedure_t procedure, void* private)
{
    element_t* elem = element_new_raw(id, rect, text, flags, procedure, private);
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
    event_t event = {.target = elem->win->surface, .type = LEVENT_FREE};
    elem->proc(elem->win, elem, &event);

    element_free_children(elem);
    free(elem->text);
    theme_override_deinit(&elem->theme);
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

void element_private_set(element_t* elem, void* private)
{
    elem->private = private;
}

void* element_private_get(element_t* elem)
{
    return elem->private;
}

element_id_t element_id_get(element_t* elem)
{
    return elem->id;
}

void element_rect_get(element_t* elem, rect_t* rect)
{
    *rect = elem->rect;
}

void element_rect_set(element_t* elem, const rect_t* rect)
{
    elem->rect = *rect;
}

void element_content_rect_get(element_t* elem, rect_t* rect)
{
    *rect = RECT_INIT_DIM(0, 0, RECT_WIDTH(&elem->rect), RECT_HEIGHT(&elem->rect));
}

void element_global_rect_get(element_t* elem, rect_t* rect)
{
    point_t point;
    element_global_point_get(elem, &point);

    *rect = RECT_INIT_DIM(point.x, point.y, RECT_WIDTH(&elem->rect), RECT_HEIGHT(&elem->rect));
}

void element_global_point_get(element_t* elem, point_t* point)
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
    element_global_point_get(elem, &point);
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
    element_global_point_get(elem, &point);
    *dest = (point_t){
        .x = point.x + src->x,
        .y = point.y + src->y,
    };
}

void element_global_to_rect(element_t* elem, rect_t* dest, const rect_t* src)
{
    point_t point;
    element_global_point_get(elem, &point);
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
    element_global_point_get(elem, &point);
    *dest = (point_t){
        .x = src->x - point.x,
        .y = src->y - point.y,
    };
}

void element_draw_begin(element_t* elem, drawable_t* draw)
{
    rect_t globalRect;
    element_global_rect_get(elem, &globalRect);

    draw->disp = elem->win->disp;
    draw->stride = RECT_WIDTH(&elem->win->rect);
    draw->buffer = &elem->win->buffer[globalRect.left + globalRect.top * draw->stride];
    draw->contentRect = RECT_INIT(0, 0, RECT_WIDTH(&elem->rect), RECT_HEIGHT(&elem->rect));
    draw->invalidRect = (rect_t){0};
}

void element_draw_end(element_t* elem, drawable_t* draw)
{
    rect_t globalInvalid;
    element_rect_to_global(elem, &globalInvalid, &draw->invalidRect);
    window_invalidate(elem->win, &globalInvalid);

    if (RECT_AREA(&draw->invalidRect) != 0)
    {
        element_t* child;
        LIST_FOR_EACH(child, &elem->children, entry)
        {
            if (RECT_OVERLAP(&draw->invalidRect, &child->rect))
            {
                element_send_redraw(child, false);
            }
        }
    }
}

void element_send_redraw(element_t* elem, bool propagate)
{
    levent_redraw_t event;
    event.id = elem->id;
    event.propagate = propagate;
    display_events_push(elem->win->disp, elem->win->surface, LEVENT_REDRAW, &event, sizeof(levent_redraw_t));
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

        if (event->lRedraw.propagate)
        {
            element_t* child;
            LIST_FOR_EACH(child, &elem->children, entry)
            {
                element_send_redraw(child, true);
            }
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

    return 0;
}

uint64_t element_emit(element_t* elem, event_type_t type, void* data, uint64_t size)
{
    event_t event = {
        .target = elem->win->surface,
        .type = type,
    };
    memcpy(event.raw, data, MIN(EVENT_MAX_DATA, size));
    return element_dispatch(elem, &event);
}

element_flags_t element_flags_get(element_t* elem)
{
    return elem->flags;
}

void element_flags_set(element_t* elem, element_flags_t flags)
{
    elem->flags = flags;
}

const char* element_text_get(element_t* elem)
{
    return elem->text;
}

uint64_t element_text_set(element_t* elem, const char* text)
{
    char* newText = strdup(text);
    if (newText == NULL)
    {
        return ERR;
    }
    free(elem->text);
    elem->text = newText;

    return 0;
}

element_text_props_t* element_text_props_get(element_t* elem)
{
    return &elem->textProps;
}

image_t* element_image_get(element_t* elem)
{
    return elem->image;
}

void element_image_set(element_t* elem, image_t* image)
{
    elem->image = image;
}

element_image_props_t* element_image_props_get(element_t* elem)
{
    return &elem->imageProps;
}

pixel_t element_color_get(element_t* elem, theme_color_set_t set, theme_color_role_t role)
{
    return theme_color_get(set, role, &elem->theme);
}

uint64_t element_color_set(element_t* elem, theme_color_set_t set, theme_color_role_t role, pixel_t color)
{
    return theme_override_color_set(&elem->theme, set, role, color);
}

const char* element_string_get(element_t* elem, theme_string_t name)
{
    return theme_string_get(name, &elem->theme);
}

uint64_t element_string_set(element_t* elem, theme_string_t name, const char* string)
{
    return theme_override_string_set(&elem->theme, name, string);
}

int64_t element_int_get(element_t* elem, theme_int_t name)
{
    return theme_int_get(name, &elem->theme);
}

uint64_t element_int_set(element_t* elem, theme_int_t name, int64_t integer)
{
    return theme_override_int_set(&elem->theme, name, integer);
}
