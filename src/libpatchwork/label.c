#include "internal.h"

#include <stdlib.h>
#include <string.h>

static uint64_t label_procedure(window_t* win, element_t* elem, const event_t* event)
{
    (void)win; // Unused

    const theme_t* theme = element_get_theme(elem);

    switch (event->type)
    {
    case LEVENT_REDRAW:
    {
        rect_t rect = element_get_content_rect(elem);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        if (!(elem->flags & ELEMENT_FLAT))
        {
            draw_frame(&draw, &rect, theme->frameSize, theme->view.shadow, theme->view.highlight);
            RECT_SHRINK(&rect, theme->frameSize);
            draw_rect(&draw, &rect, theme->view.backgroundNormal);
            RECT_SHRINK(&rect, theme->frameSize);
            // rect.top += frameSize;
        }
        else
        {
            draw_rect(&draw, &rect, theme->view.backgroundNormal);
        }

        draw_text(&draw, &rect, elem->textProps.font, elem->textProps.xAlign, elem->textProps.yAlign,
            theme->view.foregroundNormal, elem->text);

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
