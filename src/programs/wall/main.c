#include <stdio.h>
#include <stdlib.h>
#include <libdwm/dwm.h>

static uint64_t procedure(window_t* window, const event_t* event)
{
    switch (event->type)
    {
    case EVENT_INIT:
    {
        printf("init");
    }
    break;
    case EVENT_REDRAW:
    {
        rect_t rect;
        window_get_rect(window, &rect);
        //gfx_gradient(&gfx, &rect, 0xFF427F99, 0xFF5FA6C2, GFX_GRADIENT_DIAGONAL, true);*/
        window_draw_rect(window, &rect, 0xFF427F99);
    }
    break;
    }

    return 0;
}

int main(void)
{
    display_t* disp = display_open();

    rect_t rect;
    display_screen_rect(disp, &rect, 0);

    window_t* win = window_new(disp, NULL, "Wallpaper", &rect, 0, SURFACE_WALL, WINDOW_NONE, procedure);
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
