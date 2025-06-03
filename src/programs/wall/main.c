#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>

static image_t* image;

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
        if (image == NULL)
        {
            printf("wall: image failed to load\n");
            break;
        }

        rect_t rect = element_get_content_rect(elem);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        point_t srcPoint = {.x = (image_width(image) - RECT_WIDTH(&rect)) / 2,
            .y = (image_height(image) - RECT_HEIGHT(&rect)) / 2};
        draw_image(&draw, image, &rect, &srcPoint);

        element_draw_end(elem, &draw);
    }
    break;
    }

    return 0;
}

int main(void)
{
    display_t* disp = display_new();

    display_unsubscribe(disp, EVENT_KBD);
    display_unsubscribe(disp, EVENT_MOUSE);

    rect_t rect;
    display_screen_rect(disp, &rect, 0);

    image = image_new(disp, theme_get_string(STRING_WALLPAPER, NULL));

    window_t* win = window_new(disp, "Wallpaper", &rect, SURFACE_WALL, WINDOW_NONE, procedure, NULL);
    if (win == NULL)
    {
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_is_connected(disp))
    {
        display_next_event(disp, &event, CLOCKS_NEVER);
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);
    return 0;
}
