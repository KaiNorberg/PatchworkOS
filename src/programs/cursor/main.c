#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/proc.h>
#include <sys/win.h>

static fbmp_t* image;

static uint64_t procedure(win_t* window, void* private, surface_t* surface, msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        rect_t screenRect;
        win_screen_rect(&screenRect);

        lmsg_init_t* data = (lmsg_init_t*)msg->data;
        data->name = "Cursor";
        data->type = DWM_CURSOR;
        data->rect = RECT_INIT_DIM(RECT_WIDTH(&screenRect) / 2, RECT_HEIGHT(&screenRect) / 2, image->width, image->height);
    }
    break;
    case LMSG_REDRAW:
    {
        point_t point = {0};
        gfx_fbmp(surface, image, &point);
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

    win_t* cursor = win_new(procedure);
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
