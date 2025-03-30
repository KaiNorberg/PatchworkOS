#include <sys/win.h>

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_REDRAW:
    {
        gfx_t gfx;
        win_draw_begin(window, &gfx);

        rect_t rect = RECT_INIT_GFX(&gfx);

        // gfx_rect(&gfx, &rect, 0xFF007E81);
        gfx_gradient(&gfx, &rect, 0xFF427F99, 0xFF5FA6C2, GFX_GRADIENT_DIAGONAL, true);

        win_draw_end(window, &gfx);
    }
    break;
    }

    return 0;
}

int main(void)
{
    rect_t rect;
    win_screen_rect(&rect);

    win_t* window = win_new("Wallpaper", &rect, DWM_WALL, WIN_NONE, procedure);

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(window, &msg, NEVER);
        win_dispatch(window, &msg);
    }

    win_free(window);
    return 0;
}
