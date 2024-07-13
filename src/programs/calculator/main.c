#include <stdint.h>
#include <stdlib.h>
#include <sys/keyboard.h>
#include <sys/proc.h>
#include <sys/win.h>

#define WINDOW_WIDTH 350
#define WINDOW_HEIGHT 400

#define BUTTON_ID 1

static uint64_t procedure(win_t* window, void* private, surface_t* surface, msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        lmsg_init_t* data = (lmsg_init_t*)msg->data;
        data->name = "Calculator";
        data->type = DWM_WINDOW;
        data->rectIsClient = true;
        data->rect = RECT_INIT_DIM(500 * (1 + getpid() % 2), 200, WINDOW_WIDTH, WINDOW_HEIGHT);

        rect_t buttonRect = RECT_INIT_DIM(125, 50, 100, 100);
        win_widget_new(window, win_widget_button, "Press Me!", &buttonRect, BUTTON_ID);

        /*wmsg_set_font_t font = {
            .font = "/usr/fonts/zap-light16.psf"
        };
        win_widget_send(button, WMSG_SET_FONT, &font, sizeof(wmsg_set_font_t));*/
    }
    break;
    case LMSG_BUTTON:
    {
        lmsg_button_t* data = (lmsg_button_t*)msg->data;
        if (data->pressed)
        {
            if (data->id == BUTTON_ID)
            {
                asm volatile("ud2");
            }
        }
    }
    break;
    }

    return 0;
}

int main(void)
{
    win_t* window = win_new(procedure);
    if (window == NULL)
    {
        return EXIT_FAILURE;
    }

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(window, &msg, NEVER);
        win_dispatch(window, &msg);
    }

    win_free(window);
    return EXIT_SUCCESS;
}
