#include <stdlib.h>
#include <libdwm/dwm.h>

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case LEVENT_REDRAW:
    {
        /*gfx_t gfx;
        win_draw_begin(window, &gfx);

        point_t point = {0};
        gfx_fbmp(&gfx, image, &point);

        win_draw_end(window, &gfx);*/

        rect_t rect;
        element_content_rect(elem, &rect);
        element_draw_rect(elem, &rect, UINT32_MAX);
    }
    break;
    }

    return 0;
}

int main(void)
{
    display_t* disp = display_open();

    rect_t screenRect;
    display_screen_rect(disp, &screenRect, 0);

    rect_t rect = RECT_INIT_DIM(RECT_WIDTH(&screenRect) / 2, RECT_HEIGHT(&screenRect) / 2, 32, 32);

    window_t* win = window_new(disp, "Cursor", &rect, SURFACE_CURSOR, WINDOW_NONE, procedure);
    if (win == NULL)
    {
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_connected(disp))
    {
        display_next_event(disp, &event, NEVER);
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_close(disp);
    return 0;
}
