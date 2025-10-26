#include <libpatchwork/patchwork.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static image_t* image;

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    (void)win; // Unused

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
            printf("wall: image failed to load (%s)\n", strerror(errno));
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
    if (disp == NULL)
    {
        printf("wall: failed to create display (%s)\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (display_unsubscribe(disp, EVENT_KBD) == ERR)
    {
        printf("wall: failed to unsubscribe from keyboard events (%s)\n", strerror(errno));
        display_free(disp);
        return EXIT_FAILURE;
    }
    if (display_unsubscribe(disp, EVENT_MOUSE) == ERR)
    {
        printf("wall: failed to unsubscribe from mouse events (%s)\n", strerror(errno));
        display_free(disp);
        return EXIT_FAILURE;
    }

    rect_t rect = display_screen_rect(disp, 0);

    image = image_new(disp, theme_global_get()->wallpaper);
    if (image == NULL)
    {
        printf("wall: failed to load image (%s)\n", strerror(errno));
        display_free(disp);
        return EXIT_FAILURE;
    }

    window_t* win = window_new(disp, "Wallpaper", &rect, SURFACE_WALL, WINDOW_NONE, procedure, NULL);
    if (win == NULL)
    {
        printf("wall: failed to create window (%s)\n", strerror(errno));
        image_free(image);
        display_free(disp);
        return EXIT_FAILURE;
    }

    if (window_set_visible(win, true) == ERR)
    {
        printf("wall: failed to show window (%s)\n", strerror(errno));
        window_free(win);
        image_free(image);
        display_free(disp);
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_next(disp, &event, CLOCKS_NEVER) != ERR)
    {
        display_dispatch(disp, &event);
    }

    window_free(win);
    image_free(image);
    display_free(disp);
    return 0;
}
