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

    const theme_t* theme = element_get_theme(elem);

    if (elem->flags & ELEMENT_FLAT)
    {
        if (button->isPressed || button->isHovered)
        {
            draw_rect(&draw, &rect, theme->button.backgroundSelectedEnd);
        }
        else
        {
            draw_rect(&draw, &rect, theme->button.backgroundNormal);
        }
    }
    else
    {
        if (!(elem->flags & ELEMENT_NO_BEZEL))
        {
            draw_bezel(&draw, &rect, theme->bezelSize, theme->button.bezel);
            RECT_SHRINK(&rect, theme->bezelSize);
        }

        if (button->isPressed)
        {
            draw_frame(&draw, &rect, theme->frameSize, theme->button.shadow, theme->button.highlight);
        }
        else
        {
            draw_frame(&draw, &rect, theme->frameSize, theme->button.highlight, theme->button.shadow);
        }
        RECT_SHRINK(&rect, theme->frameSize);

        draw_rect(&draw, &rect, theme->button.backgroundNormal);
    }

    if (!(elem->flags & ELEMENT_NO_OUTLINE))
    {
        RECT_SHRINK(&rect, theme->smallPadding);
        if (button->isFocused)
        {
            draw_dashed_outline(&draw, &rect, theme->button.bezel, 2, 2);
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
        draw_text(&draw, &rect, elem->textProps.font, ALIGN_CENTER, ALIGN_CENTER, theme->button.foregroundSelected,
            elem->text);
    }
    else
    {
        draw_text(&draw, &rect, elem->textProps.font, ALIGN_CENTER, ALIGN_CENTER, theme->button.foregroundNormal,
            elem->text);
    }

    element_draw_end(elem, &draw);
}

static void button_send_action(element_t* elem, action_type_t type)
{
    levent_action_t event = {.source = elem->id, .type = type};
    display_push(elem->win->disp, elem->win->surface, LEVENT_ACTION, &event, sizeof(levent_action_t));
}

static uint64_t button_procedure(window_t* win, element_t* elem, const event_t* event)
{
    (void)win; // Unused

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        button_t* button = malloc(sizeof(button_t));
        if (button == NULL)
        {
            errno = ENOMEM;
            return ERR;
        }
        button->mouseButtons = 0;
        button->isPressed = false;
        button->isHovered = false;
        button->isFocused = false;

        element_set_private(elem, button);
    }
    break;
    case LEVENT_DEINIT:
    {
        button_t* button = element_get_private(elem);
        free(button);
    }
    break;
    case LEVENT_REDRAW:
    {
        button_t* button = element_get_private(elem);
        button_draw(elem, button);
    }
    break;
    case EVENT_MOUSE:
    {
        button_t* button = element_get_private(elem);
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
        button_t* button = element_get_private(elem);
        if (button->isHovered)
        {
            button->isHovered = false;
            button_draw(elem, button);
        }
    }
    break;
    case EVENT_REPORT:
    {
        button_t* button = element_get_private(elem);
        if (!(event->report.info.flags & SURFACE_FOCUSED) && button->isFocused)
        {
            button->isFocused = false;
            button_draw(elem, button);
        }
    }
    break;
    case LEVENT_FORCE_ACTION:
    {
        button_t* button = element_get_private(elem);
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
    return element_new(parent, id, rect, text, flags, button_procedure, NULL);
}
