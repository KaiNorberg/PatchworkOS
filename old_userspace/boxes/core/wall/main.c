#include <patchwork/patchwork.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libc/defs.h>

static image_t* image;

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    UNUSED(win);

    switch (event->type)
    {
    case EVENT_LIB_INIT:
    {
    }
    break;
    case EVENT_LIB_REDRAW:
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
    fd_t klog;
    status_t status = open(&klog, "/dev/klog");
    if (IS_ERR(status))
    {
        proc_exit(IOFMT("wall: failed to open klog %Y\n", status));
    }
    fd_t stdoutFd = STDOUT_FILENO;
    fd_t stderrFd = STDERR_FILENO;
    if (IS_ERR(status = dup(klog, &stdoutFd)) || IS_ERR(status = dup(klog, &stderrFd)))
    {
        close(klog);
        proc_exit(IOFMT("wall: failed to redirect stdout/stderr to klog %Y\n", status));
    }
    close(klog);

    printf("wall: initializing\n");

    display_t* disp = display_new();
    if (disp == NULL)
    {
        proc_exit(IOFMT("wall: failed to create display %Y\n", status));
    }

    printf("wall: unsubscribing to events\n");

    if (display_unsubscribe(disp, EVENT_KBD) == PFAIL)
    {
        display_free(disp);
        proc_exit(IOFMT("wall: failed to unsubscribe from keyboard events (%s)\n", strerror(errno)));
    }
    if (display_unsubscribe(disp, EVENT_MOUSE) == PFAIL)
    {
        display_free(disp);
        proc_exit(IOFMT("wall: failed to unsubscribe from mouse events (%s)\n", strerror(errno)));
    }

    printf("wall: getting screen rect\n");

    rect_t rect;
    display_get_screen(disp, &rect, 0);

    printf("wall: loading wallpaper\n");

    const theme_t* theme = theme_global_get();
    image = image_new(disp, theme->wallpaper);
    if (image == NULL)
    {
        display_free(disp);
        proc_exit(IOFMT("wall: failed to load image '%s' (%s)\n", theme->wallpaper, strerror(errno)));
    }

    printf("wall: creating window\n");

    window_t* win = window_new(disp, "Wallpaper", &rect, SURFACE_WALL, WINDOW_NONE, procedure, NULL);
    if (win == NULL)
    {
        image_free(image);
        display_free(disp);
        proc_exit(IOFMT("wall: failed to create window (%s)\n", strerror(errno)));
    }

    printf("wall: setting window visible\n");

    if (window_set_visible(win, true) == PFAIL)
    {
        window_free(win);
        image_free(image);
        display_free(disp);
        proc_exit(IOFMT("wall: failed to show window (%s)\n", strerror(errno)));
    }

    printf("wall: entering event loop\n");

    event_t event = {0};
    while (display_next(disp, &event, CLOCKS_NEVER) != PFAIL)
    {
        display_dispatch(disp, &event);
    }

    window_free(win);
    image_free(image);
    display_free(disp);
    printf("wall: exiting\n");
    return 0;
}
