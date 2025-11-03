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
    const theme_t* theme = element_get_theme(elem);

    switch (event->type)
    {
    case EVENT_LIB_INIT:
    {
        rect_t rect = element_get_content_rect(elem);

        rect_t middleButtonRect = RECT_INIT_DIM(RECT_WIDTH(&rect) / 2 - POPUP_BUTTON_WIDTH / 2,
            RECT_HEIGHT(&rect) - POPUP_BUTTON_AREA_HEIGHT + POPUP_BUTTON_HEIGHT / 2 - 10, POPUP_BUTTON_WIDTH,
            POPUP_BUTTON_HEIGHT);

        rect_t leftButtonRect = middleButtonRect;
        leftButtonRect.left -= POPUP_BUTTON_WIDTH + theme->bigPadding;
        leftButtonRect.right -= POPUP_BUTTON_WIDTH + theme->bigPadding;

        rect_t rightButtonRect = middleButtonRect;
        rightButtonRect.left += POPUP_BUTTON_WIDTH + theme->bigPadding;
        rightButtonRect.right += POPUP_BUTTON_WIDTH + theme->bigPadding;

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
    case EVENT_LIB_REDRAW:
    {
        rect_t rect = element_get_content_rect(elem);
        rect.bottom -= POPUP_BUTTON_AREA_HEIGHT;
        rect.left += POPUP_HORIZONTAL_PADDING;
        rect.right -= POPUP_HORIZONTAL_PADDING;

        drawable_t draw;
        element_draw_begin(elem, &draw);

        draw_rect(&draw, &rect, theme->deco.backgroundNormal);
        draw_text_multiline(&draw, &rect, NULL, ALIGN_MIN, ALIGN_CENTER, theme->view.foregroundNormal, popup->text);

        element_draw_end(elem, &draw);
    }
    break;
    case EVENT_LIB_ACTION:
    {
        if (event->action.type != ACTION_RELEASE)
        {
            break;
        }

        popup->result = event->action.source;
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
    display_get_screen(disp, &screenRect, 0);

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
    while (display_next(disp, &event, CLOCKS_NEVER) != ERR)
    {
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);

    return popup.result;
}
