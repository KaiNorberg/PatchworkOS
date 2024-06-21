#include "_AUX/pixel_t.h"
#include "_AUX/point_t.h"
#include "_AUX/rect_t.h"

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
    win_type_t type;
    procedure_t procedure;
    win_theme_t theme;
    char name[MAX_PATH];
} win_t;

static void win_draw_decorations(win_t* window)
{
    if (window->type == WIN_WINDOW)
    {
        surface_t surface;
        win_window_surface(window, &surface);

        rect_t localArea = (rect_t){
            .left = 0,
            .top = 0,
            .right = surface.width,
            .bottom = surface.height,
        };

        gfx_rect(&surface, &localArea, window->theme.background);
        gfx_edge(&surface, &localArea, window->theme.edgeWidth, window->theme.highlight, window->theme.shadow);

        rect_t topBar = (rect_t){
            .left = localArea.left + window->theme.edgeWidth,
            .top = localArea.top + window->theme.edgeWidth,
            .right = localArea.right - window->theme.edgeWidth,
            .bottom = localArea.top + window->theme.topbarHeight + window->theme.edgeWidth,
        };
        gfx_rect(&surface, &topBar, window->theme.topbarHighlight);

        win_flush(window, &surface);
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

static inline uint64_t win_set_area(win_t* window, const rect_t* rect)
{
    window->windowArea = *rect;

    window->clientArea = (rect_t){
        .left = 0,
        .top = 0,
        .right = RECT_WIDTH(&window->windowArea),
        .bottom = RECT_HEIGHT(&window->windowArea),
    };
    win_window_to_client(&window->clientArea, &window->theme, window->type);

    return 0;
}

uint64_t win_screen_rect(rect_t* rect)
{
    fd_t fd = open("sys:/srv/dwm");
    if (fd == ERR)
    {
        return ERR;
    }

    ioctl_dwm_size_t size;
    if (ioctl(fd, IOCTL_DWM_SIZE, &size, sizeof(ioctl_dwm_size_t)) == ERR)
    {
        return ERR;
    }

    close(fd);

    *rect = (rect_t){
        .left = 0,
        .top = 0,
        .right = size.outWidth,
        .bottom = size.outHeight,
    };
    return 0;
}

void win_client_to_window(rect_t* rect, const win_theme_t* theme, win_type_t type)
{
    if (type == WIN_WINDOW)
    {
        rect->left -= theme->edgeWidth;
        rect->top -= theme->edgeWidth + theme->topbarHeight;
        rect->right += theme->edgeWidth;
        rect->bottom += theme->edgeWidth;
    }
}

void win_window_to_client(rect_t* rect, const win_theme_t* theme, win_type_t type)
{
    if (type == WIN_WINDOW)
    {
        rect->left += theme->edgeWidth;
        rect->top += theme->edgeWidth + theme->topbarHeight;
        rect->right -= theme->edgeWidth;
        rect->bottom -= theme->edgeWidth;
    }
}

win_t* win_new(const char* name, const rect_t* rect, const win_theme_t* theme, procedure_t procedure, win_type_t type)
{
    if (strlen(name) >= MAX_PATH)
    {
        errno = EINVAL;
        return NULL;
    }

    win_t* window = malloc(sizeof(win_t));
    if (window == NULL)
    {
        return NULL;
    }

    window->fd = open("sys:/srv/dwm");
    if (window->fd == ERR)
    {
        free(window);
        return NULL;
    }

    ioctl_dwm_create_t create;
    create.x = rect->left;
    create.y = rect->top;
    create.width = RECT_WIDTH(rect);
    create.height = RECT_HEIGHT(rect);
    create.type = type;
    strcpy(create.name, name);
    if (ioctl(window->fd, IOCTL_DWM_CREATE, &create, sizeof(ioctl_dwm_create_t)) == ERR)
    {
        close(window->fd);
        free(window);
        return NULL;
    }

    window->buffer = calloc(RECT_AREA(rect), sizeof(pixel_t));
    if (window->buffer == NULL)
    {
        close(window->fd);
        free(window);
        return NULL;
    }

    window->type = type;
    window->procedure = procedure;
    window->theme = *theme;
    strcpy(window->name, name);
    win_set_area(window, rect);
    window->invalidArea = (rect_t){};

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

uint64_t win_flush(win_t* window, const surface_t* surface)
{
    uint64_t offset = ((uint64_t)surface->buffer - (uint64_t)window->buffer) / sizeof(pixel_t);
    uint64_t x = offset % surface->stride;
    uint64_t y = offset / surface->stride;

    rect_t rect = (rect_t){
        .left = x + surface->invalidArea.left,
        .top = y + surface->invalidArea.top,
        .right = x + surface->invalidArea.right,
        .bottom = y + surface->invalidArea.bottom,
    };

    if (RECT_AREA(&window->invalidArea) == 0)
    {
        window->invalidArea = rect;
    }
    else
    {
        window->invalidArea.left = MIN(window->invalidArea.left, rect.left);
        window->invalidArea.top = MIN(window->invalidArea.top, rect.top);
        window->invalidArea.right = MAX(window->invalidArea.right, rect.right);
        window->invalidArea.bottom = MAX(window->invalidArea.bottom, rect.bottom);
    }

    return 0;
}

msg_t win_dispatch(win_t* window, nsec_t timeout)
{
    ioctl_win_receive_t receive = {.timeout = timeout};
    if (ioctl(window->fd, IOCTL_WIN_RECEIVE, &receive, sizeof(ioctl_win_receive_t)) == ERR)
    {
        return LMSG_QUIT;
    }

    if (win_background_procedure(window, receive.outType, receive.outData) == ERR)
    {
        win_send(window, LMSG_QUIT, NULL, 0);
        return MSG_NONE;
    }

    if (window->procedure(window, receive.outType, receive.outData) == ERR)
    {
        win_send(window, LMSG_QUIT, NULL, 0);
        return MSG_NONE;
    }

    if (RECT_AREA(&window->invalidArea) != 0 &&
        flush(window->fd, window->buffer, RECT_AREA(&window->windowArea) * sizeof(pixel_t), &window->invalidArea) == ERR)
    {
        win_send(window, LMSG_QUIT, NULL, 0);
        return MSG_NONE;
    }
    window->invalidArea = (rect_t){};

    return receive.outType;
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

void win_window_surface(win_t* window, surface_t* surface)
{
    surface->invalidArea = (rect_t){};
    surface->buffer = window->buffer;
    surface->width = RECT_WIDTH(&window->windowArea);
    surface->height = RECT_HEIGHT(&window->windowArea);
    surface->stride = surface->width;
}

void win_client_surface(win_t* window, surface_t* surface)
{
    surface->invalidArea = (rect_t){};
    surface->width = RECT_WIDTH(&window->clientArea);
    surface->height = RECT_HEIGHT(&window->clientArea);
    surface->stride = RECT_WIDTH(&window->windowArea);
    surface->buffer = (pixel_t*)((uint64_t)window->buffer + (window->clientArea.left * sizeof(pixel_t)) +
        (window->clientArea.top * surface->stride * sizeof(pixel_t)));
}
