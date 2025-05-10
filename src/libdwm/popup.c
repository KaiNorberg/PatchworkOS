#include "internal.h"

typedef struct
{
    popup_result_t result;
    const char* text;
    popup_type_t type;
} popup_t;

static uint64_t popup_procedure(window_t* win, element_t* elem, const event_t* event)
{
    popup_t* popup = element_private(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        rect_t rect;
        element_content_rect(elem, &rect);

        rect_t middleButtonRect = RECT_INIT_DIM(RECT_WIDTH(&rect) / 2 - POPUP_BUTTON_WIDTH / 2,
            RECT_HEIGHT(&rect) - POPUP_BUTTON_AREA_HEIGHT + POPUP_BUTTON_HEIGHT / 2 - 10, POPUP_BUTTON_WIDTH,
            POPUP_BUTTON_HEIGHT);

        rect_t leftButtonRect = middleButtonRect;
        leftButtonRect.left -= POPUP_BUTTON_WIDTH + windowTheme.paddingWidth * 3;
        leftButtonRect.right -= POPUP_BUTTON_WIDTH + windowTheme.paddingWidth * 3;

        rect_t rightButtonRect = middleButtonRect;
        rightButtonRect.left += POPUP_BUTTON_WIDTH + windowTheme.paddingWidth * 3;
        rightButtonRect.right += POPUP_BUTTON_WIDTH + windowTheme.paddingWidth * 3;

        switch (popup->type)
        {
        case POPUP_OK:
        {
            button_new(elem, POPUP_RES_OK, &rightButtonRect, NULL, windowTheme.dark, windowTheme.background,
                BUTTON_NONE, "Ok");
        }
        break;
        case POPUP_RETRY_CANCEL:
        {
            button_new(elem, POPUP_RES_RETRY, &middleButtonRect, NULL, windowTheme.dark, windowTheme.background,
                BUTTON_NONE, "Retry");
            button_new(elem, POPUP_RES_CANCEL, &rightButtonRect, NULL, windowTheme.dark, windowTheme.background,
                BUTTON_NONE, "Cancel");
        }
        break;
        case POPUP_YES_NO:
        {
            button_new(elem, POPUP_RES_YES, &middleButtonRect, NULL, windowTheme.dark, windowTheme.background,
                BUTTON_NONE, "Yesy");
            button_new(elem, POPUP_RES_NO, &rightButtonRect, NULL, windowTheme.dark, windowTheme.background,
                BUTTON_NONE, "No");
        }
        break;
        }
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect(elem, &rect);
        rect.bottom -= POPUP_BUTTON_AREA_HEIGHT;

        draw_text_multiline(element_draw(elem), &rect, NULL, ALIGN_CENTER, ALIGN_CENTER, windowTheme.dark,
            windowTheme.background, popup->text);
    }
    break;
    case LEVENT_ACTION:
    {
        if (event->lAction.type != ACTION_RELEASE)
        {
            break;
        }

        popup->result = event->lAction.source;
        display_disconnect(window_display(win));
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
    while (display_connected(disp))
    {
        display_next_event(disp, &event, CLOCKS_NEVER);
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);

    return popup.result;
}
