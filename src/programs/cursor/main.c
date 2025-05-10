#include <libdwm/dwm.h>
#include <stdio.h>
#include <stdlib.h>

static image_t* image;

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

        printf("cursor: redraw");
        rect_t rect;
        element_content_rect(elem, &rect);
        point_t srcPoint = {0};
        draw_image(element_draw(elem), image, &rect, &srcPoint);
    }
    break;
    }

    return 0;
}

int main(void)
{
    display_t* disp = display_new();

    image = image_new(disp, "home:/theme/cursor/arrow.fbmp");
    if (image == NULL)
    {
        return EXIT_FAILURE;
    }

    rect_t screenRect;
    display_screen_rect(disp, &screenRect, 0);

    rect_t rect = RECT_INIT_DIM(RECT_WIDTH(&screenRect) / 2, RECT_HEIGHT(&screenRect) / 2, image_width(image),
        image_height(image));

    window_t* win = window_new(disp, "Cursor", &rect, SURFACE_CURSOR, WINDOW_NONE, procedure, NULL);
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

    printf("cursor exit");
    window_free(win);
    image_free(image);
    display_free(disp);
    return 0;
}
