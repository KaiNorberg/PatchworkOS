#include <libdwm/dwm.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case LEVENT_INIT:
    {
    }
    break;
    case LEVENT_REDRAW:
    {
        printf("wall: redraw\n");

        rect_t rect;
        element_content_rect(elem, &rect);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        draw_gradient(&draw, &rect, 0xFF427F99, 0xFF5FA6C2, GRADIENT_DIAGONAL, true);
    
        element_draw_end(elem, &draw);
    }
    break;
    }

    return 0;
}

int main(void)
{
    display_t* disp = display_new();

    rect_t rect;
    display_screen_rect(disp, &rect, 0);

    window_t* win = window_new(disp, "Wallpaper", &rect, SURFACE_WALL, WINDOW_NONE, procedure, NULL);
    if (win == NULL)
    {
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_connected(disp))
    {
        display_next_event(disp, &event, CLOCKS_NEVER);
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);
    return 0;
}
