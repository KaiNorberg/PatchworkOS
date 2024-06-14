#include "_AUX/pixel_t.h"
#include "_AUX/point_t.h"
#include "_AUX/rect_t.h"

#include "theme.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
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
    rect_t windowArea;
    rect_t clientArea;
    rect_t invalidArea;
    win_flag_t flags;
    procedure_t procedure;
} win_t;

static void win_invalidate(win_t* window, const rect_t* rect)
{
    if (rect == NULL)
    {
        window->invalidArea = (rect_t){
            .left = 0,
            .top = 0,
            .right = RECT_WIDTH(&window->windowArea),
            .bottom = RECT_HEIGHT(&window->windowArea),
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

static void win_draw_decorations(win_t* window)
{
    if (window->flags & WIN_DECO)
    {
        rect_t localArea = (rect_t){
            .left = 0,
            .top = 0,
            .right = RECT_WIDTH(&window->windowArea),
            .bottom = RECT_HEIGHT(&window->windowArea),
        };

        uint64_t width = RECT_WIDTH(&window->windowArea);
        gfx_rect(window->buffer, width, &localArea, THEME_BACKGROUND);
        gfx_edge(window->buffer, width, &localArea, THEME_EDGE_WIDTH, THEME_HIGHLIGHT, THEME_SHADOW);

        rect_t topBar = (rect_t){
            .left = localArea.left + THEME_EDGE_WIDTH,
            .top = localArea.top + THEME_EDGE_WIDTH,
            .right = localArea.right - THEME_EDGE_WIDTH,
            .bottom = localArea.top + THEME_TOPBAR_HEIGHT + THEME_EDGE_WIDTH,
        };
        gfx_rect(window->buffer, width, &topBar, THEME_TOPBAR_HIGHLIGHT);

        win_invalidate(window, NULL);
    }
}

static uint64_t win_background_procedure(win_t* window, msg_t type, void* data)
{
    switch (type)
    {
    case LMSG_REDRAW:
    {
        win_draw_decorations(window);
    }
    break;
    }

    return 0;
}

static void win_local_to_client(win_t* window, rect_t* rect)
{
    rect->left += window->clientArea.left;
    rect->top += window->clientArea.top;
    rect->right += window->clientArea.left;
    rect->bottom += window->clientArea.top;
}

static inline uint64_t win_set_area(win_t* window, const rect_t* rect)
{
    window->windowArea = *rect;

    window->clientArea = (rect_t){
        .left = 0,
        .top = 0,
        .right = RECT_WIDTH(&window->windowArea),
        .bottom = RECT_HEIGHT(&window->windowArea),
    };
    win_window_to_client(&window->clientArea, window->flags);

    return 0;
}

void win_client_to_window(rect_t* rect, win_flag_t flags)
{
    if (flags & WIN_DECO)
    {
        rect->left -= THEME_EDGE_WIDTH;
        rect->top -= THEME_EDGE_WIDTH + THEME_TOPBAR_HEIGHT;
        rect->right += THEME_EDGE_WIDTH;
        rect->bottom += THEME_EDGE_WIDTH;
    }
}

void win_window_to_client(rect_t* rect, win_flag_t flags)
{
    if (flags & WIN_DECO)
    {
        rect->left += THEME_EDGE_WIDTH;
        rect->top += THEME_EDGE_WIDTH + THEME_TOPBAR_HEIGHT;
        rect->right -= THEME_EDGE_WIDTH;
        rect->bottom -= THEME_EDGE_WIDTH;
    }
}

win_t* win_new(const rect_t* rect, procedure_t procedure, win_flag_t flags)
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

    ioctl_win_init_t init;
    init.x = rect->left;
    init.y = rect->top;
    init.width = RECT_WIDTH(rect);
    init.height = RECT_HEIGHT(rect);
    if (ioctl(window->fd, IOCTL_WIN_INIT, &init, sizeof(ioctl_win_init_t)) == ERR)
    {
        close(window->fd);
        free(window);
        return NULL;
    }

    window->flags = flags;
    window->procedure = procedure;
    win_set_area(window, rect);
    window->buffer = calloc(RECT_AREA(rect), sizeof(pixel_t));
    if (window->buffer == NULL)
    {
        close(window->fd);
        free(window);
        return NULL;
    }

    win_send(window, LMSG_INIT, NULL, 0);
    win_send(window, LMSG_REDRAW, NULL, 0);

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
    uint64_t result = flush(window->fd, window->buffer, RECT_AREA(&window->windowArea) * sizeof(pixel_t), &window->invalidArea);
    window->invalidArea = (rect_t){};
    return result;
}

msg_t win_dispatch(win_t* window, nsec_t timeout)
{
    ioctl_win_receive_t receive = {.timeout = timeout};
    if (ioctl(window->fd, IOCTL_WIN_RECEIVE, &receive, sizeof(ioctl_win_receive_t)) == ERR)
    {
        return LMSG_QUIT;
    }

    if (win_background_procedure(window, receive.type, receive.data) == ERR)
    {
        return LMSG_QUIT;
    }

    if (window->procedure(window, receive.type, receive.data) == ERR)
    {
        return LMSG_QUIT;
    }

    win_flush(window);

    return receive.type;
}

uint64_t win_send(win_t* window, msg_t type, void* data, uint64_t size)
{
    if (size >= MSG_MAX_DATA)
    {
        errno = EBUFFER;
        return ERR;
    }

    ioctl_win_send_t send;
    send.type = type;
    memset(send.data, 0, MSG_MAX_DATA);
    if (data == NULL)
    {
        memcpy(send.data, data, size);
    }

    if (ioctl(window->fd, IOCTL_WIN_SEND, &send, sizeof(ioctl_win_send_t)) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t win_move(win_t* window, const rect_t* rect)
{
    ioctl_win_move_t move;
    move.x = rect->left;
    move.y = rect->top;
    move.width = RECT_WIDTH(rect);
    move.height = RECT_HEIGHT(rect);

    if (ioctl(window->fd, IOCTL_WIN_MOVE, &move, sizeof(ioctl_win_move_t)) == ERR)
    {
        return ERR;
    }

    if (RECT_WIDTH(rect) != RECT_WIDTH(&window->windowArea) || RECT_HEIGHT(rect) != RECT_HEIGHT(&window->windowArea))
    {
        free(window->buffer);

        window->buffer = calloc(RECT_AREA(rect), sizeof(pixel_t));
        if (window->buffer == NULL)
        {
            return ERR;
        }

        win_send(window, LMSG_REDRAW, NULL, 0);
    }

    win_set_area(window, rect);

    return 0;
}

void win_window_area(win_t* window, rect_t* rect)
{
    *rect = window->windowArea;
}

void win_client_area(win_t* window, rect_t* rect)
{
    *rect = window->clientArea;
}

void win_draw_rect(win_t* window, const rect_t* rect, pixel_t pixel)
{
    rect_t clientRect = *rect;
    win_local_to_client(window, &clientRect);

    gfx_rect(window->buffer, RECT_WIDTH(&window->windowArea), &clientRect, pixel);
    win_invalidate(window, &clientRect);
}
