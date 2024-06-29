#ifndef __EMBED__

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/mouse.h>

typedef struct win win_t;

#define _WIN_INTERNAL
#include <sys/win.h>

typedef struct win
{
    fd_t fd;
    pixel_t* buffer;
    rect_t windowArea;
    rect_t clientArea;
    win_type_t type;
    procedure_t procedure;
    win_theme_t theme;
    bool selected;
    char name[MAX_PATH];
} win_t;

static void win_window_surface(win_t* window, surface_t* surface)
{
    surface->invalidArea = (rect_t){0};
    surface->buffer = window->buffer;
    surface->width = RECT_WIDTH(&window->windowArea);
    surface->height = RECT_HEIGHT(&window->windowArea);
    surface->stride = surface->width;
}

static void win_client_surface(win_t* window, surface_t* surface)
{
    surface->invalidArea = (rect_t){0};
    surface->width = RECT_WIDTH(&window->clientArea);
    surface->height = RECT_HEIGHT(&window->clientArea);
    surface->stride = RECT_WIDTH(&window->windowArea);
    surface->buffer = (pixel_t*)((uint64_t)window->buffer + (window->clientArea.left * sizeof(pixel_t)) +
        (window->clientArea.top * surface->stride * sizeof(pixel_t)));
}

static void win_draw_topbar(win_t* window, surface_t* surface)
{
    rect_t localArea = RECT_INIT_SURFACE(surface);

    rect_t topBar = (rect_t){
        .left = localArea.left + window->theme.edgeWidth,
        .top = localArea.top + window->theme.edgeWidth,
        .right = localArea.right - window->theme.edgeWidth,
        .bottom = localArea.top + window->theme.topbarHeight + window->theme.edgeWidth,
    };
    gfx_rect(surface, &topBar, window->selected ? window->theme.selected : window->theme.unSelected);
    gfx_edge(surface, &topBar, window->theme.edgeWidth, window->theme.shadow, window->theme.highlight);
}

static void win_draw_decorations(win_t* window, surface_t* surface)
{
    if (window->type == WIN_WINDOW)
    {
        rect_t localArea = RECT_INIT_SURFACE(surface);

        gfx_rect(surface, &localArea, window->theme.background);
        gfx_edge(surface, &localArea, window->theme.edgeWidth, window->theme.highlight, window->theme.shadow);

        win_draw_topbar(window, surface);
    }
}

static uint64_t win_background_procedure(win_t* window, surface_t* surface, msg_t type, void* data)
{
    switch (type)
    {
    case MSG_MOUSE:
    {
        msg_mouse_t* message = data;

        if (message->buttons & MOUSE_LEFT)
        {
            rect_t rect;
            win_window_area(window, &rect);
            rect.left += message->deltaX;
            rect.top += message->deltaY;
            rect.right += message->deltaX;
            rect.bottom += message->deltaY;

            win_move(window, &rect);
        }
    }
    break;
    case MSG_SELECT:
    {
        window->selected = true;
        win_draw_topbar(window, surface);
    }
    break;
    case MSG_DESELECT:
    {
        window->selected = false;
        win_draw_topbar(window, surface);
    }
    break;
    case LMSG_REDRAW:
    {
        win_draw_decorations(window, surface);
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
    fd_t fd = open("sys:/server/dwm");
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

    window->fd = open("sys:/server/dwm");
    if (window->fd == ERR)
    {
        free(window);
        return NULL;
    }

    ioctl_dwm_create_t create;
    create.pos.x = rect->left;
    create.pos.y = rect->top;
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

    if (theme != NULL)
    {
        window->theme = *theme;
    }
    else
    {
        memset(&window->theme, 0, sizeof(win_theme_t));
    }

    strcpy(window->name, name);
    win_set_area(window, rect);

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

msg_t win_dispatch(win_t* window, nsec_t timeout)
{
    ioctl_win_receive_t receive = {.timeout = timeout};
    if (ioctl(window->fd, IOCTL_WIN_RECEIVE, &receive, sizeof(ioctl_win_receive_t)) == ERR)
    {
        return LMSG_QUIT;
    }

    surface_t windowSurface;
    win_window_surface(window, &windowSurface);

    if (win_background_procedure(window, &windowSurface, receive.outType, receive.outData) == ERR)
    {
        win_send(window, LMSG_QUIT, NULL, 0);
        return MSG_NONE;
    }

    surface_t clientSurface;
    win_client_surface(window, &clientSurface);

    if (window->procedure(window, &clientSurface, receive.outType, receive.outData) == ERR)
    {
        win_send(window, LMSG_QUIT, NULL, 0);
        return MSG_NONE;
    }

    if (RECT_AREA(&clientSurface.invalidArea) != 0 || RECT_AREA(&windowSurface.invalidArea) != 0)
    {
        rect_t invalidRect;
        if (RECT_AREA(&windowSurface.invalidArea) == 0)
        {
            invalidRect = (rect_t){
                .left = window->clientArea.left + clientSurface.invalidArea.left,
                .top = window->clientArea.top + clientSurface.invalidArea.top,
                .right = window->clientArea.left + clientSurface.invalidArea.right,
                .bottom = window->clientArea.top + clientSurface.invalidArea.bottom,
            };
        }
        else if (RECT_AREA(&clientSurface.invalidArea) == 0)
        {
            invalidRect = windowSurface.invalidArea;
        }
        else
        {
            invalidRect = (rect_t){
                .left = MIN(window->clientArea.left + clientSurface.invalidArea.left, windowSurface.invalidArea.left),
                .top = MIN(window->clientArea.top + clientSurface.invalidArea.top, windowSurface.invalidArea.top),
                .right = MAX(window->clientArea.left + clientSurface.invalidArea.right, windowSurface.invalidArea.right),
                .bottom = MAX(window->clientArea.top + clientSurface.invalidArea.bottom, windowSurface.invalidArea.bottom),
            };
        }

        if (flush(window->fd, window->buffer, RECT_AREA(&window->windowArea) * sizeof(pixel_t), &invalidRect) == ERR)
        {
            win_send(window, LMSG_QUIT, NULL, 0);
            return MSG_NONE;
        }
    }

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

    void* newBuffer = NULL;
    if (RECT_WIDTH(rect) != RECT_WIDTH(&window->windowArea) || RECT_HEIGHT(rect) != RECT_HEIGHT(&window->windowArea))
    {
        newBuffer = calloc(RECT_AREA(rect), sizeof(pixel_t));
        if (newBuffer == NULL)
        {
            return ERR;
        }
    }

    if (ioctl(window->fd, IOCTL_WIN_MOVE, &move, sizeof(ioctl_win_move_t)) == ERR)
    {
        if (newBuffer == NULL)
        {
            free(newBuffer);
        }
        return ERR;
    }

    if (newBuffer != NULL)
    {
        free(window->buffer);
        window->buffer = newBuffer;
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

#endif
