#include "_AUX/pixel_t.h"
#include "_AUX/point_t.h"
#include "_AUX/rect_t.h"

#include "theme.h"

#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/math.h>

typedef struct win win_t;

#define _WIN_INTERNAL
#include <sys/win.h>

typedef struct win
{
    fd_t fd;
    pixel_t* buffer;
    rect_t screenArea;
    rect_t localArea;
    rect_t clientArea;
    rect_t invalidArea;
    win_flag_t flags;
    procedure_t procedure;
} win_t;

static inline void win_invalidate(win_t* window, const rect_t* rect)
{
    if (rect == NULL)
    {
        window->invalidArea = (rect_t){
            .left = 0,
            .top = 0,
            .right = RECT_WIDTH(&window->screenArea),
            .bottom = RECT_HEIGHT(&window->screenArea),
        };
    }
    else if (RECT_AREA(&window->invalidArea) == 0)
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

static inline void win_rect_to_client(win_t* window, rect_t* dest, const rect_t* src)
{
    *dest = (rect_t){
        .left = src->left + window->clientArea.left,
        .top = src->top + window->clientArea.top,
        .right = src->right + window->clientArea.left,
        .bottom = src->bottom + window->clientArea.top,
    };
}

static inline uint64_t win_set_area(win_t* window, const rect_t* rect)
{
    if (window->buffer != NULL)
    {
        free(window->buffer);
    }

    window->screenArea = *rect;

    window->localArea = (rect_t){
        .left = 0,
        .top = 0,
        .right = RECT_WIDTH(&window->screenArea),
        .bottom = RECT_HEIGHT(&window->screenArea),
    };

    if (window->flags & WIN_DECO)
    {
        window->clientArea = (rect_t){
            .left = THEME_EDGE_WIDTH,
            .top = THEME_EDGE_WIDTH + THEME_TOPBAR_HEIGHT,
            .right = RECT_WIDTH(&window->screenArea) - THEME_EDGE_WIDTH,
            .bottom = RECT_HEIGHT(&window->screenArea) - THEME_EDGE_WIDTH,
        };
    }
    else
    {
        window->clientArea = window->localArea;
    }

    window->buffer = calloc(RECT_AREA(&window->screenArea), sizeof(pixel_t));
    if (window->buffer == NULL)
    {
        return ERR;
    }

    return 0;
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

    window->buffer = NULL;
    window->flags = flags;
    window->procedure = procedure;

    if (window->flags & WIN_DECO)
    {
        info->width += THEME_EDGE_WIDTH * 2;
        info->height += THEME_EDGE_WIDTH * 2 + THEME_TOPBAR_HEIGHT;
    }

    if (ioctl(window->fd, IOCTL_WIN_INIT, info, sizeof(ioctl_win_init_t)) == ERR)
    {
        close(window->fd);
        free(window);
        return NULL;
    }

    rect_t screenArea = (rect_t){
        .left = info->x,
        .top = info->y,
        .right = info->x + info->width,
        .bottom = info->y + info->height,
    };
    win_set_area(window, &screenArea);

    // Temp
    if (window->flags & WIN_DECO)
    {
        uint64_t width = RECT_WIDTH(&window->screenArea);
        gfx_rect(window->buffer, width, &window->localArea, THEME_BACKGROUND);
        gfx_edge(window->buffer, width, &window->localArea, THEME_EDGE_WIDTH, THEME_HIGHLIGHT, THEME_SHADOW);

        rect_t topBar = (rect_t){
            .left = window->localArea.left + THEME_EDGE_WIDTH,
            .top = window->localArea.top + THEME_EDGE_WIDTH,
            .right = window->localArea.right - THEME_EDGE_WIDTH,
            .bottom = window->localArea.top + THEME_TOPBAR_HEIGHT + THEME_EDGE_WIDTH,
        };
        gfx_rect(window->buffer, width, &topBar, THEME_TOPBAR_HIGHLIGHT);
    }

    win_invalidate(window, NULL);
    win_flush(window);

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
    uint64_t result = flush(window->fd, window->buffer, RECT_AREA(&window->screenArea) * sizeof(pixel_t), &window->invalidArea);
    window->invalidArea = (rect_t){};
    return result;
}

void win_screen_area(win_t* window, rect_t* rect)
{
    *rect = window->screenArea;
}

void win_local_area(win_t* window, rect_t* rect)
{
    *rect = window->localArea;
}

void win_client_area(win_t* window, rect_t* rect)
{
    *rect = window->clientArea;
}

msg_t win_dispatch(win_t* window, nsec_t timeout)
{
    ioctl_win_dispatch_t dispatch = {.timeout = timeout};
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

void win_draw_rect(win_t* window, const rect_t* rect, pixel_t pixel)
{
    rect_t clientRect;
    win_rect_to_client(window, &clientRect, rect);

    gfx_rect(window->buffer, RECT_WIDTH(&window->screenArea), &clientRect, pixel);
    win_invalidate(window, &clientRect);
}
