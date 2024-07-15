#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/proc.h>
#include <sys/win.h>

static fbmp_t* image;

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_REDRAW:
    {
        surface_t surface;
        win_draw_begin(window, &surface);

        point_t point = {0};
        gfx_fbmp(&surface, image, &point);

        win_draw_end(window, &surface);
    }
    break;
    }

    return 0;
}

int main(void)
{
    image = gfx_load_fbmp("/usr/cursor/arrow.fbmp");
    if (image == NULL)
    {
        exit(EXIT_FAILURE);
    }

    rect_t screenRect;
    win_screen_rect(&screenRect);
    rect_t rect = RECT_INIT_DIM(RECT_WIDTH(&screenRect) / 2, RECT_HEIGHT(&screenRect) / 2, image->width, image->height);
    win_t* cursor = win_new("Cursor", DWM_CURSOR, &rect, procedure);
    if (cursor == NULL)
    {
        return EXIT_FAILURE;
    }

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(cursor, &msg, NEVER);
        win_dispatch(cursor, &msg);
    }

    win_free(cursor);
    return EXIT_SUCCESS;
}
