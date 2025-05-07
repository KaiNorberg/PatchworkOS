#include "internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct label
{
    font_t* font;
    align_t xAlign;
    align_t yAlign;
    pixel_t foreground;
    pixel_t background;
    label_flags_t flags;
    char* text;
    element_t* elem;
} label_t;

static uint64_t label_procedure(window_t* win, element_t* elem, const event_t* event)
{
    label_t* label = element_private(elem);
    switch (event->type)
    {
    case LEVENT_INIT:
    {
    }
    break;
    case LEVENT_FREE:
    {
        free(label->text);
        free(label);
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect(elem, &rect);

        drawable_t* draw = element_draw(elem);

        if (!(label->flags & LABEL_FLAT))
        {
            draw_edge(draw, &rect, windowTheme.edgeWidth, windowTheme.shadow, windowTheme.highlight);
            RECT_SHRINK(&rect, windowTheme.edgeWidth);
            draw_rect(draw, &rect, label->background);
            RECT_SHRINK(&rect, windowTheme.edgeWidth);
            rect.top += windowTheme.edgeWidth;
        }

        draw_text(draw, &rect, label->font, label->xAlign, label->yAlign, label->foreground, label->background,
            label->text);
    }
    break;
    }

    return 0;
}

label_t* label_new(element_t* parent, element_id_t id, const rect_t* rect, font_t* font, align_t xAlign, align_t yAlign,
    pixel_t foreground, pixel_t background, label_flags_t flags, const char* text)
{
    label_t* label = malloc(sizeof(label_t));
    if (label == NULL)
    {
        return NULL;
    }

    label->font = font;
    label->xAlign = xAlign;
    label->yAlign = yAlign;
    label->foreground = foreground;
    label->background = background;
    label->flags = flags;
    label->text = strdup(text);
    if (label->text == NULL)
    {
        free(label);
        return NULL;
    }

    label->elem = element_new(parent, id, rect, label_procedure, label);
    if (label->elem == NULL)
    {
        free(label);
        return NULL;
    }
    return label;
}

void label_free(label_t* label)
{
    element_free(label->elem);
}

font_t* label_font(label_t* label)
{
    return label->font;
}

void label_set_font(label_t* label, font_t* font)
{
    label->font = font;
    element_send_redraw(label->elem, false);
}

align_t label_xalign(label_t* label)
{
    return label->xAlign;
}

void label_set_xalign(label_t* label, align_t xAlign)
{
    label->xAlign = xAlign;
    element_send_redraw(label->elem, false);
}

align_t label_yalign(label_t* label)
{
    return label->yAlign;
}

void label_set_yalign(label_t* label, align_t yAlign)
{
    label->xAlign = yAlign;
    element_send_redraw(label->elem, false);
}

pixel_t label_foreground(label_t* label)
{
    return label->foreground;
}

void label_set_foreground(label_t* label, pixel_t foreground)
{
    label->foreground = foreground;
    element_send_redraw(label->elem, false);
}

pixel_t label_background(label_t* label)
{
    return label->background;
}

void label_set_background(label_t* label, pixel_t background)
{
    label->background = background;
    element_send_redraw(label->elem, false);
}

label_flags_t label_flags(label_t* label)
{
    return label->flags;
}

void label_set_flags(label_t* label, label_flags_t flags)
{
    label->flags = flags;
    element_send_redraw(label->elem, false);
}

const char* label_text(label_t* label)
{
    return label->text;
}

uint64_t label_set_text(label_t* label, const char* text)
{
    char* newText = strdup(text);
    if (newText == NULL)
    {
        return ERR;
    }
    free(label->text);
    label->text = newText;

    element_send_redraw(label->elem, false);
    return 0;
}
