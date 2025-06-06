#include "internal.h"

typedef struct
{
    popup_result_t result;
    const char* text;
    popup_type_t type;
} popup_t;

static uint64_t popup_procedure(window_t* win, element_t* elem, const event_t* event)
{
    popup_t* popup = element_get_private(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        rect_t rect = element_get_content_rect(elem);

        rect_t middleButtonRect = RECT_INIT_DIM(RECT_WIDTH(&rect) / 2 - POPUP_BUTTON_WIDTH / 2,
            RECT_HEIGHT(&rect) - POPUP_BUTTON_AREA_HEIGHT + POPUP_BUTTON_HEIGHT / 2 - 10, POPUP_BUTTON_WIDTH,
            POPUP_BUTTON_HEIGHT);

        int64_t bigPadding = element_get_int(elem, INT_BIG_PADDING);

        rect_t leftButtonRect = middleButtonRect;
        leftButtonRect.left -= POPUP_BUTTON_WIDTH + bigPadding;
        leftButtonRect.right -= POPUP_BUTTON_WIDTH + bigPadding;

        rect_t rightButtonRect = middleButtonRect;
        rightButtonRect.left += POPUP_BUTTON_WIDTH + bigPadding;
        rightButtonRect.right += POPUP_BUTTON_WIDTH + bigPadding;

        switch (popup->type)
        {
        case POPUP_OK:
        {
            button_new(elem, POPUP_RES_OK, &rightButtonRect, "Ok", ELEMENT_NONE);
        }
        break;
        case POPUP_RETRY_CANCEL:
        {
            button_new(elem, POPUP_RES_RETRY, &middleButtonRect, "Retry", ELEMENT_NONE);
            button_new(elem, POPUP_RES_CANCEL, &rightButtonRect, "Cancel", ELEMENT_NONE);
        }
        break;
        case POPUP_YES_NO:
        {
            button_new(elem, POPUP_RES_YES, &middleButtonRect, "Yes", ELEMENT_NONE);
            button_new(elem, POPUP_RES_NO, &rightButtonRect, "No", ELEMENT_NONE);
        }
        break;
        }
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect = element_get_content_rect(elem);
        rect.bottom -= POPUP_BUTTON_AREA_HEIGHT;
        rect.left += POPUP_HORIZONTAL_PADDING;
        rect.right -= POPUP_HORIZONTAL_PADDING;

        drawable_t draw;
        element_draw_begin(elem, &draw);

        pixel_t foreground = element_get_color(elem, COLOR_SET_VIEW, COLOR_ROLE_FOREGROUND_NORMAL);
        pixel_t background = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_NORMAL);
        draw_rect(&draw, &rect, background);
        draw_text_multiline(&draw, &rect, NULL, ALIGN_MIN, ALIGN_CENTER, foreground, popup->text);

        element_draw_end(elem, &draw);
    }
    break;
    case LEVENT_ACTION:
    {
        if (event->lAction.type != ACTION_RELEASE)
        {
            break;
        }

        popup->result = event->lAction.source;
        display_disconnect(window_get_display(win));
    }
    break;
    }

    return 0;
}

popup_result_t popup_open(const char* text, const char* title, popup_type_t type)
{
    display_t* disp = display_new();
    if (disp == NULL)
    {
        return POPUP_RES_ERROR;
    }

    rect_t screenRect;
    display_screen_rect(disp, &screenRect, 0);

    popup_t popup = {
        .result = POPUP_RES_CLOSE,
        .text = text,
        .type = type,
    };

    rect_t rect = RECT_INIT_DIM(RECT_WIDTH(&screenRect) / 2 - POPUP_WIDTH / 2,
        RECT_HEIGHT(&screenRect) / 2 - POPUP_HEIGHT / 2, POPUP_WIDTH, POPUP_HEIGHT);
    window_t* win =
        window_new(disp, title, &rect, SURFACE_WINDOW, WINDOW_DECO | WINDOW_NO_CONTROLS, popup_procedure, &popup);
    if (win == NULL)
    {
        return POPUP_RES_ERROR;
    }

    event_t event = {0};
    while (display_is_connected(disp))
    {
        display_next_event(disp, &event, CLOCKS_NEVER);
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);

    return popup.result;
}
