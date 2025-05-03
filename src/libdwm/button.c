#include "internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct button
{
    font_t* font;
    pixel_t foreground;
    pixel_t background;
    button_flags_t flags;
    char* label;
    mouse_buttons_t pressed;
    element_t* elem;
} button_t;

static void button_draw(button_t* button, bool redraw)
{
    rect_t rect;
    element_content_rect(button->elem, &rect);

    if (redraw)
    {
        element_draw_rim(button->elem, &rect, windowTheme.rimWidth, windowTheme.dark);
    }
    RECT_SHRINK(&rect, windowTheme.rimWidth);

    if (button->pressed)
    {
        element_draw_edge(button->elem, &rect, windowTheme.edgeWidth, windowTheme.shadow, windowTheme.highlight);
    }
    else
    {
        element_draw_edge(button->elem, &rect, windowTheme.edgeWidth, windowTheme.highlight, windowTheme.shadow);
    }
    RECT_SHRINK(&rect, windowTheme.edgeWidth);

    if (redraw)
    {
        element_draw_rect(button->elem, &rect, button->background);
        element_draw_text(button->elem, &rect, button->font, ALIGN_CENTER, ALIGN_CENTER, button->foreground, button->background, button->label);
    }
}

static void button_send_action(button_t* button, action_type_t type)
{
    levent_action_t event = {.source = button->elem->id, .type = type};
    display_events_push(button->elem->win->disp, button->elem->win->id, LEVENT_ACTION, &event, sizeof(levent_action_t));
}

#include <stdio.h>

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
        free(button->label);
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
        point_t cursorPos;
        element_global_to_point(elem, &cursorPos, &event->mouse.pos); // WRONG SPACE

        if (button->flags & BUTTON_TOGGLE)
        {
            if (RECT_CONTAINS_POINT(&rect, &cursorPos) && event->mouse.pressed & MOUSE_LEFT)
            {
                button->pressed = !button->pressed;
                button_send_action(button, button->pressed ? ACTION_PRESS : ACTION_RELEASE);
            }
        }
        else
        {
            if (RECT_CONTAINS_POINT(&rect, &cursorPos))
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

button_t* button_new(element_t* parent, element_id_t id, const rect_t* rect, font_t* font, pixel_t foreground, pixel_t background,
    button_flags_t flags, const char* label)
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
    button->label = strdup(label);
    if (button->label == NULL)
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

const char* button_label(button_t* button)
{
    return button->label;
}

uint64_t button_set_label(button_t* button, const char* label)
{
    char* newLabel = strdup(label);
    if (newLabel == NULL)
    {
        return ERR;
    }
    free(button->label);
    button->label = newLabel;

    element_send_redraw(button->elem);
    return 0;
}

font_t* button_font(button_t* button)
{
    return button->font;
}

void button_set_font(button_t* button, font_t* font)
{
    button->font = font;
}
