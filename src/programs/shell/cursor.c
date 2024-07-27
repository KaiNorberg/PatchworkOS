#include "cursor.h"

#include <stdlib.h>
#include <sys/win.h>
#include <sys/gfx.h>

static gfx_fbmp_t* image;

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_REDRAW:
    {
        gfx_t gfx;
        win_draw_begin(window, &gfx);

        point_t point = {0};
        gfx_fbmp(&gfx, image, &point);

        win_draw_end(window, &gfx);
    }
    break;
    }

    return 0;
}

win_t* cursor_new(void)
{
    image = gfx_fbmp_load("/usr/cursor/arrow.fbmp");
    if (image == NULL)
    {
        exit(EXIT_FAILURE);
    }

    rect_t screenRect;
    win_screen_rect(&screenRect);
    rect_t rect = RECT_INIT_DIM(RECT_WIDTH(&screenRect) / 2, RECT_HEIGHT(&screenRect) / 2, image->width, image->height);
    return win_new("Cursor", &rect, DWM_CURSOR, WIN_NONE, procedure);
}
