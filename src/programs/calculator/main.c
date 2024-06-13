#include <stdint.h>
#include <stdlib.h>
#include <sys/kbd.h>
#include <sys/proc.h>
#include <sys/win.h>

uint64_t procedure(win_t* window, msg_t type, void* data)
{
    switch (type)
    {
    case MSG_INIT:
    {
        rect_t rect;
        win_local_rect(window, &rect);
        gfx_rect(window, &rect, 0xFFFF0000);
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
    info.width = 250;
    info.height = 500;

    win_t* window = win_new(&info, procedure, 0);
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
