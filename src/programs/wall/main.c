#include <libdwm/dwm.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case EVENT_INIT:
    {
        printf("wall: init");
    }
    break;
    case EVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect(elem, &rect);
        // gfx_gradient(&gfx, &rect, 0xFF427F99, 0xFF5FA6C2, GFX_GRADIENT_DIAGONAL, true);*/
        element_draw_rect(elem, &rect, 0xFF427F99);
    }
    break;
    }

    return 0;
}

int main(void)
{
    display_t* disp = display_open();

    printf("wall: display_screen_rect");
    rect_t rect;
    display_screen_rect(disp, &rect, 0);

    printf("wall: window_new");
    window_t* win = window_new(disp, "Wallpaper", &rect, SURFACE_WALL, WINDOW_NONE, procedure);
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

    printf("wall: window_free");
    window_free(win);
    display_close(disp);
    return 0;
}
