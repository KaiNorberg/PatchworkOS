#include "internal.h"

#include <stdlib.h>
#include <string.h>

static uint64_t element_send_init(element_t* elem)
{
    event_t event = {.target = elem->win->surface, .type = EVENT_LIB_INIT};
    if (elem->proc(elem->win, elem, &event) == ERR)
    {
        return ERR;
    }

    element_redraw(elem, false);
    return 0;
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
    elem->imageProps = (image_props_t){.xAlign = ALIGN_CENTER, .yAlign = ALIGN_CENTER, .srcOffset = (point_t){0}};
    elem->textProps = (text_props_t){.xAlign = ALIGN_CENTER, .yAlign = ALIGN_CENTER, .font = NULL};
    elem->theme = *theme_global_get();
    return elem;
}

element_t* element_new(element_t* parent, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags,
    procedure_t procedure, void* private)
{
    if (parent == NULL || rect == NULL || text == NULL || procedure == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    element_t* elem = element_new_raw(id, rect, text, flags, procedure, private);
    if (elem == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    elem->parent = parent;
    elem->win = parent->win;
    list_push(&parent->children, &elem->entry);

    if (element_send_init(elem) == ERR)
    {
        element_free(elem);
        return NULL;
    }
    return elem;
}

element_t* element_new_root(window_t* win, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags,
    procedure_t procedure, void* private)
{
    if (win == NULL || rect == NULL || text == NULL || procedure == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    element_t* elem = element_new_raw(id, rect, text, flags, procedure, private);
    if (elem == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    elem->win = win;

    if (element_send_init(elem) == ERR)
    {
        element_free(elem);
        return NULL;
    }
    return elem;
}

static void element_free_children(element_t* elem)
{
    element_t* child;
    element_t* temp;
    LIST_FOR_EACH_SAFE(child, temp, &elem->children, entry)
    {
        list_remove(&elem->children, &child->entry);
        element_free(child);
    }
}

void element_free(element_t* elem)
{
    if (elem == NULL)
    {
        return;
    }

    event_t event = {.target = elem->win->surface, .type = EVENT_LIB_DEINIT};
    elem->proc(elem->win, elem, &event);

    if (elem->parent != NULL)
    {
        list_remove(&elem->parent->children, &elem->entry);
        elem->parent = NULL;
    }

    element_free_children(elem);
    free(elem->text);
    free(elem);
}

element_t* element_find(element_t* elem, element_id_t id)
{
    if (elem == NULL)
    {
        return NULL;
    }

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
    if (elem == NULL)
    {
        return;
    }

    elem->private = private;
}

void* element_get_private(element_t* elem)
{
    if (elem == NULL)
    {
        return NULL;
    }

    return elem->private;
}

element_id_t element_get_id(element_t* elem)
{
    if (elem == NULL)
    {
        return ELEMENT_ID_NONE;
    }

    return elem->id;
}

void element_move(element_t* elem, const rect_t* rect)
{
    if (elem == NULL || rect == NULL)
    {
        return;
    }

    elem->rect = *rect;
}

rect_t element_get_rect(element_t* elem)
{
    if (elem == NULL)
    {
        return (rect_t){0};
    }

    return elem->rect;
}

rect_t element_get_content_rect(element_t* elem)
{
    if (elem == NULL)
    {
        return (rect_t){0};
    }

    return RECT_INIT_DIM(0, 0, RECT_WIDTH(&elem->rect), RECT_HEIGHT(&elem->rect));
}

rect_t element_get_window_rect(element_t* elem)
{
    if (elem == NULL)
    {
        return (rect_t){0};
    }

    point_t point = element_get_window_point(elem);
    return RECT_INIT_DIM(point.x, point.y, RECT_WIDTH(&elem->rect), RECT_HEIGHT(&elem->rect));
}

point_t element_get_window_point(element_t* elem)
{
    if (elem == NULL)
    {
        return (point_t){0};
    }

    point_t offset = {.x = elem->rect.left, .y = elem->rect.top};
    element_t* parent = elem->parent;
    while (parent != NULL)
    {
        offset.x += parent->rect.left;
        offset.y += parent->rect.top;
        parent = parent->parent;
    }

    return offset;
}

rect_t element_rect_to_window(element_t* elem, const rect_t* src)
{
    if (elem == NULL || src == NULL)
    {
        return (rect_t){0};
    }

    point_t point = element_get_window_point(elem);
    return (rect_t){
        .left = point.x + src->left,
        .top = point.y + src->top,
        .right = point.x + src->right,
        .bottom = point.y + src->bottom,
    };
}

point_t element_point_to_window(element_t* elem, const point_t* src)
{
    if (elem == NULL || src == NULL)
    {
        return (point_t){0};
    }

    point_t point = element_get_window_point(elem);
    return (point_t){
        .x = point.x + src->x,
        .y = point.y + src->y,
    };
}

rect_t element_window_to_rect(element_t* elem, const rect_t* src)
{
    if (elem == NULL || src == NULL)
    {
        return (rect_t){0};
    }

    point_t point = element_get_window_point(elem);
    return (rect_t){
        .left = src->left - point.x,
        .top = src->top - point.y,
        .right = src->right - point.x,
        .bottom = src->bottom - point.y,
    };
}

point_t element_window_to_point(element_t* elem, const point_t* src)
{
    if (elem == NULL || src == NULL)
    {
        return (point_t){0};
    }

    point_t point = element_get_window_point(elem);
    return (point_t){
        .x = src->x - point.x,
        .y = src->y - point.y,
    };
}

element_flags_t element_flags_get(element_t* elem)
{
    if (elem == NULL)
    {
        return ELEMENT_NONE;
    }

    return elem->flags;
}

void element_set_flags(element_t* elem, element_flags_t flags)
{
    if (elem == NULL)
    {
        return;
    }

    elem->flags = flags;
}

const char* element_text_get(element_t* elem)
{
    if (elem == NULL)
    {
        return NULL;
    }

    return elem->text;
}

uint64_t element_set_text(element_t* elem, const char* text)
{
    if (elem == NULL || text == NULL)
    {
        return ERR;
    }

    char* newText = strdup(text);
    if (newText == NULL)
    {
        return ERR;
    }
    free(elem->text);
    elem->text = newText;

    return 0;
}

text_props_t* element_get_text_props(element_t* elem)
{
    if (elem == NULL)
    {
        return NULL;
    }

    return &elem->textProps;
}

image_t* element_image_get(element_t* elem)
{
    if (elem == NULL)
    {
        return NULL;
    }

    return elem->image;
}

void element_set_image(element_t* elem, image_t* image)
{
    if (elem == NULL)
    {
        return;
    }

    elem->image = image;
}

image_props_t* element_image_props_get(element_t* elem)
{
    if (elem == NULL)
    {
        return NULL;
    }

    return &elem->imageProps;
}

theme_t* element_get_theme(element_t* elem)
{
    if (elem == NULL)
    {
        return NULL;
    }

    return &elem->theme;
}

void element_draw_begin(element_t* elem, drawable_t* draw)
{
    if (elem == NULL || draw == NULL)
    {
        return;
    }

    rect_t globalRect = element_get_window_rect(elem);

    draw->disp = elem->win->disp;
    draw->stride = RECT_WIDTH(&elem->win->rect);
    draw->buffer = &elem->win->buffer[globalRect.left + globalRect.top * draw->stride];
    draw->contentRect = RECT_INIT(0, 0, RECT_WIDTH(&elem->rect), RECT_HEIGHT(&elem->rect));
    draw->invalidRect = (rect_t){0};
}

void element_draw_end(element_t* elem, drawable_t* draw)
{
    if (elem == NULL || draw == NULL)
    {
        return;
    }

    rect_t globalInvalid = element_rect_to_window(elem, &draw->invalidRect);
    window_invalidate(elem->win, &globalInvalid);

    if (RECT_AREA(&draw->invalidRect) != 0)
    {
        element_t* child;
        LIST_FOR_EACH(child, &elem->children, entry)
        {
            if (RECT_OVERLAP(&draw->invalidRect, &child->rect))
            {
                element_redraw(child, false);
            }
        }
    }
}

void element_redraw(element_t* elem, bool shouldPropagate)
{
    if (elem == NULL)
    {
        return;
    }

    event_lib_redraw_t event;
    event.id = elem->id;
    event.shouldPropagate = shouldPropagate;
    display_push(elem->win->disp, elem->win->surface, EVENT_LIB_REDRAW, &event, sizeof(event_lib_redraw_t));
}

void element_force_action(element_t* elem, action_type_t action)
{
    if (elem == NULL)
    {
        return;
    }

    event_lib_force_action_t event;
    event.dest = elem->id;
    event.action = action;
    display_push(elem->win->disp, elem->win->surface, EVENT_LIB_FORCE_ACTION, &event, sizeof(event_lib_force_action_t));
}

uint64_t element_dispatch(element_t* elem, const event_t* event)
{
    if (elem == NULL || event == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (event->type)
    {
    case EVENT_LIB_INIT:
    {
        if (elem->proc(elem->win, elem, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    case EVENT_LIB_REDRAW:
    {
        if (elem->proc(elem->win, elem, event) == ERR)
        {
            return ERR;
        }

        if (event->redraw.shouldPropagate)
        {
            element_t* child;
            LIST_FOR_EACH(child, &elem->children, entry)
            {
                element_emit(child, event->type, event->raw, EVENT_MAX_DATA);
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

uint64_t element_emit(element_t* elem, event_type_t type, const void* data, uint64_t size)
{
    if (elem == NULL || (data == NULL && size > 0) || size > EVENT_MAX_DATA)
    {
        errno = EINVAL;
        return ERR;
    }

    event_t event = {
        .target = elem->win->surface,
        .type = type,
    };
    memcpy(event.raw, data, MIN(EVENT_MAX_DATA, size));
    return element_dispatch(elem, &event);
}
