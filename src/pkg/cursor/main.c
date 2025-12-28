#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>

static image_t* image;

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    (void)win; // Unused

    switch (event->type)
    {
    case EVENT_LIB_REDRAW:
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
    fd_t klog = open("/dev/klog");
    if (klog == ERR)
    {
        printf("cursor: failed to open klog\n");
        return EXIT_FAILURE;
    }
    if (dup2(klog, STDOUT_FILENO) == ERR || dup2(klog, STDERR_FILENO) == ERR)
    {
        printf("cursor: failed to redirect stdout/stderr to klog\n");
        close(klog);
        return EXIT_FAILURE;
    }
    close(klog);

    display_t* disp = display_new();
    if (disp == NULL)
    {
        printf("cursor: failed to create display\n");
        return EXIT_FAILURE;
    }

    const theme_t* theme = theme_global_get();
    image = image_new(disp, theme->cursorArrow);
    if (image == NULL)
    {
        printf("cursor: failed to load cursor image\n");
        return EXIT_FAILURE;
    }

    rect_t screenRect;
    display_get_screen(disp, &screenRect, 0);
    rect_t rect = RECT_INIT_DIM(RECT_WIDTH(&screenRect) / 2, RECT_HEIGHT(&screenRect) / 2, image_width(image),
        image_height(image));

    window_t* win = window_new(disp, "Cursor", &rect, SURFACE_CURSOR, WINDOW_NONE, procedure, NULL);
    if (win == NULL)
    {
        printf("cursor: failed to create window\n");
        return EXIT_FAILURE;
    }

    if (window_set_visible(win, true) == ERR)
    {
        printf("cursor: failed to show window\n");
        window_free(win);
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
