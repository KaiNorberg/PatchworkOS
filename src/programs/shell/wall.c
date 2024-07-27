#include "wall.h"

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_REDRAW:
    {
        gfx_t gfx;
        win_draw_begin(window, &gfx);

        rect_t rect = RECT_INIT_GFX(&gfx);
        gfx_rect(&gfx, &rect, 0xFF007E81);
        // gfx_rect(&gfx, &rect, 0xFF3E77B3);

        win_draw_end(window, &gfx);
    }
    break;
    }

    return 0;
}

win_t* wall_new(void)
{
    rect_t rect;
    win_screen_rect(&rect);
    return win_new("Wallpaper", &rect, DWM_WALL, WIN_NONE, procedure);
}
