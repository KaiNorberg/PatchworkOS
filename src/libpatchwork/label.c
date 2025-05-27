#include "internal.h"

#include <stdlib.h>
#include <string.h>

static uint64_t label_procedure(window_t* win, element_t* elem, const event_t* event)
{
    int64_t bezelSize = element_int_get(elem, INT_BEZEL_SIZE);
    int64_t frameSize = element_int_get(elem, INT_FRAME_SIZE);
    pixel_t bezelColor = element_color_get(elem, COLOR_SET_VIEW, COLOR_ROLE_BEZEL);
    pixel_t highlight = element_color_get(elem, COLOR_SET_VIEW, COLOR_ROLE_HIGHLIGHT);
    pixel_t shadow = element_color_get(elem, COLOR_SET_VIEW, COLOR_ROLE_SHADOW);
    pixel_t background = element_color_get(elem, COLOR_SET_VIEW, COLOR_ROLE_BACKGROUND_NORMAL);
    pixel_t foreground = element_color_get(elem, COLOR_SET_VIEW, COLOR_ROLE_FOREGROUND_NORMAL);

    switch (event->type)
    {
    case LEVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect_get(elem, &rect);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        if (!(elem->flags & ELEMENT_FLAT))
        {
            draw_frame(&draw, &rect, frameSize, shadow, highlight);
            RECT_SHRINK(&rect, frameSize);
            draw_rect(&draw, &rect, background);
            RECT_SHRINK(&rect, frameSize);
            // rect.top += frameSize;
        }
        else
        {
            draw_rect(&draw, &rect, background);
        }

        draw_text(&draw, &rect, elem->textProps.font, elem->textProps.xAlign, elem->textProps.yAlign, foreground,
            elem->text);

        element_draw_end(elem, &draw);
    }
    break;
    }

    return 0;
}

element_t* label_new(element_t* parent, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags)
{
    return element_new(parent, id, rect, text, flags, label_procedure, NULL);
}