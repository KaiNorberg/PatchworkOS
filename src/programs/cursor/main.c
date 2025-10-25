#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>

static image_t* image;

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    (void)win; // Unused

    switch (event->type)
    {
    case LEVENT_REDRAW:
    {
        rect_t rect = element_get_content_rect(elem);
        point_t srcPoint = {0};

        drawable_t draw;
        element_draw_begin(elem, &draw);

        draw_image(&draw, image, &rect, &srcPoint);
        element_draw_end(elem, &draw);
    }
    break;
    }

    return 0;
}

int main(void)
{
    const theme_t* theme = theme_global_get();
    if (theme == NULL)
    {
        printf("cursor: failed to get global theme\n");
        return EXIT_FAILURE;
    }

    display_t* disp = display_new();
    if (disp == NULL)
    {
        printf("cursor: failed to create display\n");
        return EXIT_FAILURE;
    }

    image = image_new(disp, theme->cursorArrow);
    if (image == NULL)
    {
        printf("cursor: failed to load cursor image\n");
        return EXIT_FAILURE;
    }

    rect_t screenRect = display_screen_rect(disp, 0);
    rect_t rect = RECT_INIT_DIM(RECT_WIDTH(&screenRect) / 2, RECT_HEIGHT(&screenRect) / 2, image_width(image),
        image_height(image));

    window_t* win = window_new(disp, "Cursor", &rect, SURFACE_CURSOR, WINDOW_NONE, procedure, NULL);
    if (win == NULL)
    {
        printf("cursor: failed to create window\n");
        return EXIT_FAILURE;
    }

    window_set_visible(win, true);

    event_t event = {0};
    while (display_next_event(disp, &event, CLOCKS_NEVER) != ERR)
    {
        display_dispatch(disp, &event);
    }

    window_free(win);
    image_free(image);
    display_free(disp);
    return 0;
}
