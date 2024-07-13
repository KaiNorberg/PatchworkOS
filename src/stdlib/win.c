#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/mouse.h>

#include "internal/win_background.h"
#include "internal/win_internal.h"

#ifndef __EMBED__

static uint64_t win_widget_recieve(widget_t* widget, msg_t* msg);
static uint64_t win_widget_dispatch(widget_t* widget, surface_t* surface, msg_t* msg);

static void win_window_surface(win_t* window, surface_t* surface)
{
    surface->invalidArea = (rect_t){0};
    surface->buffer = window->buffer;
    surface->width = window->width;
    surface->height = window->height;
    surface->stride = surface->width;
}

static void win_client_surface(win_t* window, surface_t* surface)
{
    surface->invalidArea = (rect_t){0};
    surface->width = RECT_WIDTH(&window->clientArea);
    surface->height = RECT_HEIGHT(&window->clientArea);
    surface->stride = window->width;
    surface->buffer = (pixel_t*)((uint64_t)window->buffer + (window->clientArea.left * sizeof(pixel_t)) +
        (window->clientArea.top * surface->stride * sizeof(pixel_t)));
}

static uint64_t win_set_area(win_t* window, const rect_t* rect)
{
    window->pos = (point_t){.x = rect->left, .y = rect->top};
    window->width = RECT_WIDTH(rect);
    window->height = RECT_HEIGHT(rect);

    if (window->type == DWM_WINDOW)
    {
        window->clientArea = (rect_t){
            .left = theme.edgeWidth,
            .top = theme.edgeWidth + theme.topbarHeight,
            .right = window->width - theme.edgeWidth,
            .bottom = window->height - theme.edgeWidth,
        };
    }
    else
    {
        window->clientArea = (rect_t){
            .left = 0,
            .top = 0,
            .right = window->width,
            .bottom = window->height,
        };
    }

    return 0;
}

win_t* win_new(win_proc_t procedure)
{
    win_t* window = malloc(sizeof(win_t));
    if (window == NULL)
    {
        return NULL;
    }

    window->procedure = procedure;
    list_init(&window->widgets);
    window->moving = false;
    window->selected = false;

    msg_t initMsg = {.type = LMSG_INIT};
    lmsg_init_t* initData = (lmsg_init_t*)initMsg.data;
    initData->name = NULL;
    initData->type = DWM_WINDOW;
    initData->rect = RECT_INIT(0, 0, 0, 0);
    initData->rectIsClient = false;
    initData->private = NULL;
    if (window->procedure(window, NULL, NULL, &initMsg) == ERR || initData->name == NULL || RECT_AREA(&initData->rect) == 0 ||
        strlen(initData->name) >= DWM_MAX_NAME)
    {
        free(window);
        return NULL;
    }

    if (initData->rectIsClient)
    {
        if (window->type == DWM_WINDOW)
        {
            initData->rect.left -= theme.edgeWidth;
            initData->rect.top -= theme.edgeWidth + theme.topbarHeight;
            initData->rect.right += theme.edgeWidth;
            initData->rect.bottom += theme.edgeWidth;
        }
    }
    strcpy(window->name, initData->name);
    window->type = initData->type;
    window->private = initData->private;
    win_set_area(window, &initData->rect);

    window->fd = open("sys:/server/dwm");
    if (window->fd == ERR)
    {
        free(window);
        return NULL;
    }

    ioctl_dwm_create_t create;
    create.pos.x = initData->rect.left;
    create.pos.y = initData->rect.top;
    create.width = RECT_WIDTH(&initData->rect);
    create.height = RECT_HEIGHT(&initData->rect);
    create.type = initData->type;
    strcpy(create.name, initData->name);
    if (ioctl(window->fd, IOCTL_DWM_CREATE, &create, sizeof(ioctl_dwm_create_t)) == ERR)
    {
        close(window->fd);
        free(window);
        return NULL;
    }

    window->buffer = calloc(create.width * create.height, sizeof(pixel_t));
    if (window->buffer == NULL)
    {
        close(window->fd);
        free(window);
        return NULL;
    }

    win_send(window, LMSG_REDRAW, NULL, 0);

    return window;
}

uint64_t win_free(win_t* window)
{
    if (close(window->fd) == ERR)
    {
        return ERR;
    }

    widget_t* widget;
    LIST_FOR_EACH(widget, &window->widgets)
    {
        msg_t msg = {.type = WMSG_FREE};
        win_widget_dispatch(widget, NULL, &msg);
    }

    msg_t msg = {.type = LMSG_FREE};
    window->procedure(window, NULL, NULL, &msg);

    free(window->buffer);
    free(window);
    return 0;
}

uint64_t win_send(win_t* window, msg_type_t type, const void* data, uint64_t size)
{
    if (size >= MSG_MAX_DATA)
    {
        errno = EINVAL;
        return ERR;
    }

    ioctl_window_send_t send = {.msg.type = type};
    memcpy(send.msg.data, data, size);

    if (ioctl(window->fd, IOCTL_WINDOW_SEND, &send, sizeof(ioctl_window_send_t)) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t win_receive(win_t* window, msg_t* msg, nsec_t timeout)
{
    ioctl_window_receive_t receive = {.timeout = timeout};
    if (ioctl(window->fd, IOCTL_WINDOW_RECEIVE, &receive, sizeof(ioctl_window_receive_t)) == ERR)
    {
        return ERR;
    }

    *msg = receive.outMsg;
    return receive.outMsg.type != MSG_NONE;
}

uint64_t win_dispatch(win_t* window, msg_t* msg)
{
    surface_t windowSurface;
    win_window_surface(window, &windowSurface);
    surface_t clientSurface;
    win_client_surface(window, &clientSurface);

    win_background_procedure(window, &windowSurface, msg);
    uint64_t result = window->procedure(window, window->private, &clientSurface, msg);

    widget_t* widget;
    LIST_FOR_EACH(widget, &window->widgets)
    {
        while (widget->readIndex != widget->writeIndex)
        {
            win_widget_dispatch(widget, &clientSurface, &widget->messages[widget->readIndex]);
            widget->readIndex = (widget->readIndex + 1) % WIN_WIDGET_MAX_MSG;
        }
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

        if (flush(window->fd, window->buffer, window->width * window->height * sizeof(pixel_t), &invalidRect) == ERR)
        {
            return ERR;
        }
    }

    return result;
}

uint64_t win_move(win_t* window, const rect_t* rect)
{
    ioctl_window_move_t move;
    move.pos.x = rect->left;
    move.pos.y = rect->top;
    move.width = RECT_WIDTH(rect);
    move.height = RECT_HEIGHT(rect);

    void* newBuffer = NULL;
    if (window->width != move.width || window->height != move.height)
    {
        newBuffer = calloc(move.width * move.height, sizeof(pixel_t));
        if (newBuffer == NULL)
        {
            return ERR;
        }
    }

    if (ioctl(window->fd, IOCTL_WINDOW_MOVE, &move, sizeof(ioctl_window_move_t)) == ERR)
    {
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

void win_screen_window_area(win_t* window, rect_t* rect)
{
    *rect = RECT_INIT_DIM(window->pos.x, window->pos.y, window->width, window->height);
}

void win_screen_client_area(win_t* window, rect_t* rect)
{
    *rect = (rect_t){
        .left = window->pos.x + window->clientArea.left,
        .top = window->pos.y + window->clientArea.top,
        .right = window->pos.x + window->clientArea.right,
        .bottom = window->pos.y + window->clientArea.bottom,
    };
}

void win_client_area(win_t* window, rect_t* rect)
{
    *rect = window->clientArea;
}

void win_screen_to_window(win_t* window, point_t* point)
{
    point->x += window->pos.x;
    point->y += window->pos.y;
}

void win_screen_to_client(win_t* window, point_t* point)
{
    point->x += window->pos.x + window->clientArea.left;
    point->y += window->pos.y + window->clientArea.top;
}

void win_window_to_client(win_t* window, point_t* point)
{
    point->x += window->clientArea.left;
    point->y += window->clientArea.top;
}

widget_t* win_widget_new(win_t* window, widget_proc_t procedure, const char* name, const rect_t* rect, widget_id_t id)
{
    if (strlen(name) >= DWM_MAX_NAME)
    {
        return NULL;
    }

    widget_t* widget = malloc(sizeof(widget_t));
    list_entry_init(&widget->base);
    widget->id = id;
    widget->procedure = procedure;
    widget->rect = *rect;
    widget->window = window;
    widget->readIndex = 0;
    widget->writeIndex = 0;
    widget->private = NULL;
    strcpy(widget->name, name);
    list_push(&window->widgets, widget);

    msg_t initMsg = {.type = LMSG_INIT};
    if (win_widget_dispatch(widget, NULL, &initMsg) == ERR)
    {
        list_remove(widget);
        free(widget);
        return NULL;
    }

    win_widget_send(widget, WMSG_REDRAW, NULL, 0);

    return widget;
}

void win_widget_free(widget_t* widget)
{
    msg_t freeMsg = {.type = WMSG_FREE};
    win_widget_dispatch(widget, NULL, &freeMsg);

    list_remove(widget);
    free(widget);
}

uint64_t win_widget_send(widget_t* widget, msg_type_t type, const void* data, uint64_t size)
{
    widget->messages[widget->writeIndex].type = type;
    memcpy(widget->messages[widget->writeIndex].data, data, size);
    widget->writeIndex = (widget->writeIndex + 1) % WIN_WIDGET_MAX_MSG;

    return 0;
}

static uint64_t win_widget_dispatch(widget_t* widget, surface_t* surface, msg_t* msg)
{
    return widget->procedure(widget, widget->private, widget->window, surface, msg);
}

void win_widget_rect(widget_t* widget, rect_t* rect)
{
    *rect = widget->rect;
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

#endif

void win_theme(win_theme_t* winTheme)
{
    *winTheme = theme;
}
