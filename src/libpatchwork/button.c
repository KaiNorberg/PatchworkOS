#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct button
{
    mouse_buttons_t mouseButtons;
    bool isPressed;
    bool isHovered;
    bool isFocused;
} button_t;

static void button_draw(element_t* elem, button_t* button)
{
    rect_t rect = element_get_content_rect(elem);

    drawable_t draw;
    element_draw_begin(elem, &draw);

    int64_t bezelSize = element_get_int(elem, INT_BEZEL_SIZE);
    int64_t frameSize = element_get_int(elem, INT_FRAME_SIZE);
    pixel_t smallPadding = element_get_int(elem, INT_SMALL_PADDING);
    pixel_t bezelColor = element_get_color(elem, COLOR_SET_BUTTON, COLOR_ROLE_BEZEL);
    pixel_t highlight = element_get_color(elem, COLOR_SET_BUTTON, COLOR_ROLE_HIGHLIGHT);
    pixel_t shadow = element_get_color(elem, COLOR_SET_BUTTON, COLOR_ROLE_SHADOW);
    pixel_t background = element_get_color(elem, COLOR_SET_BUTTON, COLOR_ROLE_BACKGROUND_NORMAL);
    pixel_t foreground = element_get_color(elem, COLOR_SET_BUTTON, COLOR_ROLE_FOREGROUND_NORMAL);
    pixel_t selectedStart = element_get_color(elem, COLOR_SET_BUTTON, COLOR_ROLE_BACKGROUND_SELECTED_START);
    pixel_t selectedEnd = element_get_color(elem, COLOR_SET_BUTTON, COLOR_ROLE_BACKGROUND_SELECTED_END);
    pixel_t selectedForeground = element_get_color(elem, COLOR_SET_BUTTON, COLOR_ROLE_FOREGROUND_SELECTED);

    if (elem->flags & ELEMENT_FLAT)
    {
        if (button->isPressed || button->isHovered)
        {
            draw_rect(&draw, &rect, selectedEnd);
        }
        else
        {
            draw_rect(&draw, &rect, background);
        }
    }
    else
    {
        if (!(elem->flags & ELEMENT_NO_BEZEL))
        {
            draw_bezel(&draw, &rect, bezelSize, bezelColor);
            RECT_SHRINK(&rect, bezelSize);
        }

        if (button->isPressed)
        {
            draw_frame(&draw, &rect, frameSize, shadow, highlight);
        }
        else
        {
            draw_frame(&draw, &rect, frameSize, highlight, shadow);
        }
        RECT_SHRINK(&rect, frameSize);

        draw_rect(&draw, &rect, background);
    }

    if (!(elem->flags & ELEMENT_NO_OUTLINE))
    {
        RECT_SHRINK(&rect, smallPadding);
        if (button->isFocused)
        {
            draw_outline(&draw, &rect, bezelColor, 2, 2);
        }
        RECT_SHRINK(&rect, 2);
    }

    if (elem->image != NULL)
    {
        rect_t imageDestRect;
        int32_t imageWidth = image_width(elem->image);
        int32_t imageHeight = image_height(elem->image);

        switch (elem->imageProps.xAlign)
        {
        case ALIGN_MIN:
        {
            imageDestRect.left = rect.left;
        }
        break;
        case ALIGN_CENTER:
        {
            imageDestRect.left = rect.left + (RECT_WIDTH(&rect) - imageWidth) / 2;
        }
        break;
        case ALIGN_MAX:
        {
            imageDestRect.left = rect.left + RECT_WIDTH(&rect) - imageWidth;
        }
        break;
        default:
        {
            imageDestRect.left = rect.left + (RECT_WIDTH(&rect) - imageWidth) / 2;
        }
        break;
        }

        switch (elem->imageProps.yAlign)
        {
        case ALIGN_MIN:
        {
            imageDestRect.top = rect.top;
        }
        break;
        case ALIGN_CENTER:
        {
            imageDestRect.top = rect.top + (RECT_HEIGHT(&rect) - imageHeight) / 2;
        }
        break;
        case ALIGN_MAX:
        {
            imageDestRect.top = rect.top + RECT_HEIGHT(&rect) - imageHeight;
        }
        break;
        default:
        {
            imageDestRect.top = rect.top + (RECT_HEIGHT(&rect) - imageHeight) / 2;
        }
        break;
        }

        imageDestRect.right = imageDestRect.left + imageWidth;
        imageDestRect.bottom = imageDestRect.top + imageHeight;

        draw_image_blend(&draw, elem->image, &imageDestRect, &elem->imageProps.srcOffset);
    }

    if ((elem->flags & ELEMENT_FLAT) && (button->isHovered || button->isPressed))
    {
        draw_text(&draw, &rect, elem->textProps.font, ALIGN_CENTER, ALIGN_CENTER, selectedForeground, elem->text);
    }
    else
    {
        draw_text(&draw, &rect, elem->textProps.font, ALIGN_CENTER, ALIGN_CENTER, foreground, elem->text);
    }

    element_draw_end(elem, &draw);
}

static void button_send_action(element_t* elem, action_type_t type)
{
    levent_action_t event = {.source = elem->id, .type = type};
    display_events_push(elem->win->disp, elem->win->surface, LEVENT_ACTION, &event, sizeof(levent_action_t));
}

static uint64_t button_procedure(window_t* win, element_t* elem, const event_t* event)
{
    (void)win; // Unused

    button_t* button = element_get_private(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
    }
    break;
    case LEVENT_FREE:
    {
        free(button);
    }
    break;
    case LEVENT_REDRAW:
    {
        button_draw(elem, button);
    }
    break;
    case EVENT_MOUSE:
    {
        bool prevIsPressed = button->isPressed;
        bool prevIsHovered = button->isHovered;
        bool prevIsFocused = button->isFocused;

        rect_t rect = element_get_content_rect(elem);

        bool mouseInBounds = RECT_CONTAINS_POINT(&rect, &event->mouse.pos);
        bool leftPressed = (event->mouse.pressed & MOUSE_LEFT) != 0;
        bool leftReleased = (event->mouse.released & MOUSE_LEFT) != 0;

        if (elem->flags & ELEMENT_TOGGLE)
        {
            if (mouseInBounds)
            {
                button->isHovered = true;

                if (leftPressed)
                {
                    button->isPressed = !button->isPressed;
                    button->isFocused = true;

                    button_send_action(elem, button->isPressed ? ACTION_PRESS : ACTION_RELEASE);
                }
            }
            else
            {
                button->isHovered = false;

                if (leftPressed)
                {
                    button->isFocused = false;
                }
            }
        }
        else
        {
            if (mouseInBounds)
            {
                button->isHovered = true;

                if (leftPressed && !button->isPressed)
                {
                    button->isPressed = true;
                    button->isFocused = true;
                    button_send_action(elem, ACTION_PRESS);
                }
                else if (leftReleased && button->isPressed)
                {
                    button->isPressed = false;
                    button_send_action(elem, ACTION_RELEASE);
                }
            }
            else
            {
                button->isHovered = false;

                if (button->isPressed)
                {
                    button->isPressed = false;
                    button_send_action(elem, ACTION_CANCEL);
                }

                if (leftPressed)
                {
                    button->isFocused = false;
                }
            }
        }

        if (button->isPressed != prevIsPressed || button->isHovered != prevIsHovered ||
            button->isFocused != prevIsFocused)
        {
            button_draw(elem, button);
        }
    }
    break;
    case EVENT_CURSOR_LEAVE:
    {
        if (button->isHovered)
        {
            button->isHovered = false;
            button_draw(elem, button);
        }
    }
    break;
    case EVENT_REPORT:
    {
        if (!event->report.info.isFocused && button->isFocused)
        {
            button->isFocused = false;
            button_draw(elem, button);
        }
    }
    break;
    case LEVENT_FORCE_ACTION:
    {
        switch (event->lForceAction.action)
        {
        case ACTION_PRESS:
        {
            button->isPressed = true;
            button->isFocused = true;
        }
        break;
        case ACTION_RELEASE:
        {
            button->isPressed = false;
            button->isFocused = false;
        }
        break;
        default:
        {
        }
        }
        button_draw(elem, button);
    }
    break;
    }

    return 0;
}

element_t* button_new(element_t* parent, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags)
{
    button_t* button = malloc(sizeof(button_t));
    if (button == NULL)
    {
        return NULL;
    }
    button->isPressed = false;
    button->isHovered = false;
    button->isFocused = false;

    element_t* elem = element_new(parent, id, rect, text, flags, button_procedure, button);
    if (elem == NULL)
    {
        free(button);
        return NULL;
    }
    return elem;
}
