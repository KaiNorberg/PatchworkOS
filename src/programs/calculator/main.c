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
    case MSG_INIT:
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
    case MSG_QUIT:
    {
    }
    break;
    case MSG_KBD:
    {
        msg_kbd_t* msg = data;
    }
    break;
    }

    return 0;
}

int main(void)
{
    ioctl_win_init_t info;
    info.x = 500;
    info.y = 200;
    info.width = WINDOW_WIDTH;
    info.height = WINDOW_HEIGHT;

    win_t* window = win_new(&info, procedure, WIN_DECO);
    if (window == NULL)
    {
        exit(EXIT_FAILURE);
    }

    while (win_dispatch(window, NEVER) != MSG_QUIT)
    {
    }

    win_free(window);

    return EXIT_SUCCESS;
}
