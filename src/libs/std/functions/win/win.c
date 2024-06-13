#include "_AUX/pixel_t.h"
#include "_AUX/point_t.h"
#include "_AUX/rect_t.h"

#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>

typedef struct win win_t;

#define _WIN_INTERNAL
#include <sys/win.h>

typedef struct win
{
    fd_t fd;
    pixel_t* buffer;
    rect_t invalidArea;
    point_t pos;
    uint64_t width;
    uint64_t height;
    procedure_t procedure;
} win_t;

static inline void win_invalidate(win_t* window, const rect_t* rect)
{
    if (RECT_AREA(&window->invalidArea) == 0)
    {
        window->invalidArea = *rect;
    }
    else
    {
        window->invalidArea.left = MIN(window->invalidArea.left, rect->left);
        window->invalidArea.top = MIN(window->invalidArea.top, rect->top);
        window->invalidArea.right = MAX(window->invalidArea.right, rect->right);
        window->invalidArea.bottom = MAX(window->invalidArea.bottom, rect->bottom);
    }
}

win_t* win_new(ioctl_win_init_t* info, procedure_t procedure, win_flag_t flags)
{
    win_t* window = malloc(sizeof(win_t));
    if (window == NULL)
    {
        return NULL;
    }

    window->fd = open("sys:/srv/win");
    if (window->fd == ERR)
    {
        free(window);
        return NULL;
    }

    if (ioctl(window->fd, IOCTL_WIN_INIT, info, sizeof(ioctl_win_init_t)) == ERR)
    {
        close(window->fd);
        free(window);
        return NULL;
    }

    window->buffer = calloc(info->height * info->width, sizeof(pixel_t));
    if (window->buffer == NULL)
    {
        close(window->fd);
        free(window);
        return NULL;
    }

    window->pos = (point_t){
        .x = info->x,
        .y = info->y,
    };
    window->width = info->width;
    window->height = info->height;
    window->invalidArea = (rect_t){};
    window->procedure = procedure;

    return window;
}

uint64_t win_free(win_t* window)
{
    if (close(window->fd) == ERR)
    {
        return ERR;
    }

    free(window->buffer);
    free(window);
    return 0;
}

uint64_t win_flush(win_t* window)
{
    uint64_t result = flush(window->fd, window->buffer, window->width * window->height * sizeof(pixel_t), NULL);
    window->invalidArea = (rect_t){};
    return result;
}

void win_screen_rect(win_t* window, rect_t* rect)
{
    *rect = (rect_t){
        .left = window->pos.x,
        .top = window->pos.y,
        .right = window->pos.x + window->width,
        .bottom = window->pos.y + window->height,
    };
}

void win_local_rect(win_t* window, rect_t* rect)
{
    *rect = (rect_t){
        .left = 0,
        .top = 0,
        .right = window->width,
        .bottom = window->height,
    };
}

msg_t win_dispatch(win_t* window, nsec_t timeout)
{
    ioctl_win_dispatch_t dispatch = {
        .timeout = timeout,
    };
    if (ioctl(window->fd, IOCTL_WIN_DISPATCH, &dispatch, sizeof(ioctl_win_dispatch_t)) == ERR)
    {
        return ERR;
    }

    if (window->procedure(window, dispatch.type, dispatch.data) == ERR)
    {
        // TODO: Quit here
    }

    win_flush(window);

    return dispatch.type;
}

uint64_t gfx_rect(win_t* window, const rect_t* rect, pixel_t pixel)
{
    for (uint64_t x = rect->left; x < rect->right; x++)
    {
        for (uint64_t y = rect->top; y < rect->bottom; y++)
        {
            window->buffer[x + y * window->width] = pixel;
        }
    }

    win_invalidate(window, rect);

    return 0;
}
