#include "internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct button
{
    font_t* font;
    pixel_t foreground;
    pixel_t background;
    button_flags_t flags;
    char* text;
    mouse_buttons_t pressed;
    element_t* elem;
} button_t;

static void button_draw(button_t* button, bool redraw)
{
    rect_t rect;
    element_content_rect(button->elem, &rect);

    drawable_t draw;
    element_draw_begin(button->elem, &draw);

    if (redraw)
    {
        draw_rim(&draw, &rect, windowTheme.rimWidth, windowTheme.dark);
    }
    RECT_SHRINK(&rect, windowTheme.rimWidth);

    if (button->pressed)
    {
        draw_edge(&draw, &rect, windowTheme.edgeWidth, windowTheme.shadow, windowTheme.highlight);
    }
    else
    {
        draw_edge(&draw, &rect, windowTheme.edgeWidth, windowTheme.highlight, windowTheme.shadow);
    }
    RECT_SHRINK(&rect, windowTheme.edgeWidth);

    if (redraw)
    {
        draw_rect(&draw, &rect, button->background);
        draw_text(&draw, &rect, button->font, ALIGN_CENTER, ALIGN_CENTER, button->foreground, 0, button->text);
    }

    element_draw_end(button->elem, &draw);
}

static void button_send_action(button_t* button, action_type_t type)
{
    levent_action_t event = {.source = button->elem->id, .type = type};
    display_events_push(button->elem->win->disp, button->elem->win->surface, LEVENT_ACTION, &event,
        sizeof(levent_action_t));
}

static uint64_t button_prodecure(window_t* win, element_t* elem, const event_t* event)
{
    button_t* button = element_private(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
    }
    break;
    case LEVENT_FREE:
    {
        free(button->text);
        free(button);
    }
    break;
    case LEVENT_REDRAW:
    {
        button_draw(button, true);
    }
    break;
    case EVENT_MOUSE:
    {
        bool prevPressed = button->pressed;

        rect_t rect;
        element_content_rect(elem, &rect);

        if (button->flags & BUTTON_TOGGLE)
        {
            if (RECT_CONTAINS_POINT(&rect, &event->mouse.pos) && event->mouse.pressed & MOUSE_LEFT)
            {
                button->pressed = !button->pressed;
                button_send_action(button, button->pressed ? ACTION_PRESS : ACTION_RELEASE);
            }
        }
        else
        {
            if (RECT_CONTAINS_POINT(&rect, &event->mouse.pos))
            {
                if (event->mouse.pressed & MOUSE_LEFT && !button->pressed)
                {
                    button->pressed = true;
                    button_send_action(button, ACTION_PRESS);
                }
                else if (event->mouse.released & MOUSE_LEFT && button->pressed)
                {
                    button->pressed = false;
                    button_send_action(button, ACTION_RELEASE);
                }
            }
            else
            {
                button->pressed = false;
            }
        }

        if (button->pressed != prevPressed)
        {
            button_draw(button, false);
        }
    }
    break;
    }

    return 0;
}

button_t* button_new(element_t* parent, element_id_t id, const rect_t* rect, font_t* font, pixel_t foreground,
    pixel_t background, button_flags_t flags, const char* text)
{
    button_t* button = malloc(sizeof(button_t));
    if (button == NULL)
    {
        return NULL;
    }

    button->font = font;
    button->foreground = foreground;
    button->background = background;
    button->flags = flags;
    button->text = strdup(text);
    if (button->text == NULL)
    {
        free(button);
        return NULL;
    }
    button->pressed = MOUSE_NONE;

    button->elem = element_new(parent, id, rect, button_prodecure, button);
    if (button->elem == NULL)
    {
        free(button);
        return NULL;
    }
    return button;
}

void button_free(button_t* button)
{
    element_free(button->elem);
}

font_t* button_font(button_t* button)
{
    return button->font;
}

void button_set_font(button_t* button, font_t* font)
{
    button->font = font;
    element_send_redraw(button->elem, false);
}

pixel_t button_foreground(button_t* button)
{
    return button->foreground;
}

void button_set_foreground(button_t* button, pixel_t foreground)
{
    button->foreground = foreground;
    element_send_redraw(button->elem, false);
}

pixel_t button_background(button_t* button)
{
    return button->background;
}

void button_set_background(button_t* button, pixel_t background)
{
    button->background = background;
    element_send_redraw(button->elem, false);
}

button_flags_t button_flags(button_t* button)
{
    return button->flags;
}

void button_set_flags(button_t* button, button_flags_t flags)
{
    button->flags = flags;
    element_send_redraw(button->elem, false);
}

const char* button_text(button_t* button)
{
    return button->text;
}

uint64_t button_set_text(button_t* button, const char* text)
{
    char* newText = strdup(text);
    if (newText == NULL)
    {
        return ERR;
    }
    free(button->text);
    button->text = newText;

    element_send_redraw(button->elem, false);
    return 0;
}
