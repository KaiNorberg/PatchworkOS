#include <stdint.h>
#include <stdlib.h>
#include <sys/kbd.h>
#include <sys/proc.h>
#include <sys/win.h>

#define WINDOW_WIDTH 350
#define WINDOW_HEIGHT 500

uint64_t procedure(win_t* window, msg_t type, void* data)
{
    switch (type)
    {
    case LMSG_INIT:
    {
    }
    break;
    case LMSG_REDRAW:
    {
        rect_t rect = (rect_t){
            .left = 5,
            .top = 5,
            .right = WINDOW_WIDTH - 5,
            .bottom = WINDOW_HEIGHT - 5,
        };
        win_draw_rect(window, &rect, 0xFFFF00FF);
    }
    break;
    case LMSG_QUIT:
    {
    }
    break;
    }

    return 0;
}

int main(void)
{
    rect_t rect = (rect_t){
        .left = 500,
        .top = 200,
        .right = 500 + WINDOW_WIDTH,
        .bottom = 200 + WINDOW_HEIGHT,
    };
    win_client_to_window(&rect, WIN_DECO);

    win_t* window = win_new(&rect, procedure, WIN_DECO);
    if (window == NULL)
    {
        exit(EXIT_FAILURE);
    }

    while (win_dispatch(window, NEVER) != LMSG_QUIT);

    win_free(window);

    return EXIT_SUCCESS;
}
