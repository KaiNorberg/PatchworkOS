#include "win/display.h"
#include <win/win.h>

/*static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_REDRAW:
    {
        rect_t rect = RECT_INIT_GFX(&gfx);

        gfx_rect(&gfx, &rect, 0xFF007E81);
        //gfx_gradient(&gfx, &rect, 0xFF427F99, 0xFF5FA6C2, GFX_GRADIENT_DIAGONAL, true);
    }
    break;
    }

    return 0;
}*/

#include <stdio.h>

int main(void)
{
    display_t* disp = display_open();

    rect_t rect;
    display_screen_rect(disp, &rect, 0);
    printf("%d, %d, %d, %d", rect.left, rect.top, rect.right, rect.bottom);

    //win_t* win = win_new(disp, "Wallpaper)

    display_close(disp);
    //win_t* window = win_new();

    /*rect_t rect;
    win_screen_rect(&rect);

    win_t* window = win_new("Wallpaper", &rect, DWM_WALL, WIN_NONE, procedure);

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(window, &msg, NEVER);
        win_dispatch(window, &msg);
    }

    win_free(window);
    dwm_close(dwm);
    return 0;*/
}
