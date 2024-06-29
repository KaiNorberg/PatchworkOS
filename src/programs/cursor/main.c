#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/proc.h>
#include <sys/win.h>

static fbmp_t* image;

static uint64_t procedure(win_t* window, surface_t* surface, msg_t type, void* data)
{
    switch (type)
    {
    case LMSG_REDRAW:
    {
        point_t point = {0};
        gfx_fbmp(surface, image, &point);
    }
    break;
    }

    return 0;
}

static win_t* cursor_create(void)
{
    rect_t screenRect;
    win_screen_rect(&screenRect);
    rect_t rect = RECT_INIT_DIM(RECT_WIDTH(&screenRect) / 2, RECT_HEIGHT(&screenRect) / 2, image->width, image->height);

    win_t* cursor = win_new("Cursor", &rect, NULL, procedure, WIN_CURSOR);
    if (cursor == NULL)
    {
        exit(EXIT_FAILURE);
    }

    return cursor;
}

static void cursor_loop(win_t* cursor)
{
    while (win_dispatch(cursor, NEVER) != LMSG_QUIT)
    {
    }
}

int main(void)
{
    image = gfx_load_fbmp("home:/usr/cursor/arrow.fbmp");
    if (image == NULL)
    {
        exit(EXIT_FAILURE);
    }

    win_t* cursor = cursor_create();

    cursor_loop(cursor);

    win_free(cursor);

    return EXIT_SUCCESS;
}
