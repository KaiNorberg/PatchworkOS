#ifndef __EMBED__

#include "internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/mouse.h>

static uint64_t win_widget_dispatch(widget_t* widget, const msg_t* msg);

static uint64_t win_set_area(win_t* window, const rect_t* rect)
{
    window->pos = (point_t){.x = rect->left, .y = rect->top};
    window->width = RECT_WIDTH(rect);
    window->height = RECT_HEIGHT(rect);

    window->clientArea = RECT_INIT_DIM(0, 0, window->width, window->height);
    win_shrink_to_client(&window->clientArea, window->type);

    return 0;
}

win_t* win_new(const char* name, dwm_type_t type, const rect_t* rect, win_proc_t procedure)
{
    if (RECT_AREA(rect) == 0 || strlen(name) >= DWM_MAX_NAME || name == NULL)
    {
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
        return NULL;
    }

    window->buffer = calloc(create.width * create.height, sizeof(pixel_t));
    if (window->buffer == NULL)
    {
        close(window->fd);
        return NULL;
    }

    window->type = type;
    list_init(&window->widgets);
    window->selected = false;
    window->moving = false;
    window->procedure = procedure;
    strcpy(window->name, name);
    win_set_area(window, rect);

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
        win_widget_dispatch(widget, &msg);
    }

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

uint64_t win_dispatch(win_t* window, const msg_t* msg)
{
    win_background_procedure(window, msg);
    uint64_t result = window->procedure(window, msg);

    widget_t* widget;
    LIST_FOR_EACH(widget, &window->widgets)
    {
        while (widget->readIndex != widget->writeIndex)
        {
            win_widget_dispatch(widget, &widget->messages[widget->readIndex]);
            widget->readIndex = (widget->readIndex + 1) % WIN_WIDGET_MAX_MSG;
        }
    }

    return result;
}

uint64_t win_draw_begin(win_t* window, surface_t* surface)
{
    win_client_surface(window, surface);
    return 0;
}

uint64_t win_draw_end(win_t* window, surface_t* surface)
{
    rect_t rect = (rect_t){
        .left = window->clientArea.left + surface->invalidArea.left,
        .top = window->clientArea.top + surface->invalidArea.top,
        .right = window->clientArea.left + surface->invalidArea.right,
        .bottom = window->clientArea.top + surface->invalidArea.bottom,
    };

    if (flush(window->fd, window->buffer, window->width * window->height * sizeof(pixel_t), &rect) == ERR)
    {
        return ERR;
    }

    return 0;
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
    point->x -= window->pos.x;
    point->y -= window->pos.y;
}

void win_screen_to_client(win_t* window, point_t* point)
{
    point->x -= window->pos.x + window->clientArea.left;
    point->y -= window->pos.y + window->clientArea.top;
}

void win_window_to_client(win_t* window, point_t* point)
{
    point->x -= window->clientArea.left;
    point->y -= window->clientArea.top;
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
    widget->private = NULL;
    widget->readIndex = 0;
    widget->writeIndex = 0;
    strcpy(widget->name, name);
    list_push(&window->widgets, widget);

    win_widget_send(widget, WMSG_INIT, NULL, 0);
    win_widget_send(widget, WMSG_REDRAW, NULL, 0);

    return widget;
}

void win_widget_free(widget_t* widget)
{
    msg_t freeMsg = {.type = WMSG_FREE};
    win_widget_dispatch(widget, &freeMsg);

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

uint64_t win_widget_send_all(win_t* window, msg_type_t type, const void* data, uint64_t size)
{
    widget_t* widget;
    LIST_FOR_EACH(widget, &window->widgets)
    {
        win_widget_send(widget, type, data, size);
    }

    return 0;
}

static uint64_t win_widget_dispatch(widget_t* widget, const msg_t* msg)
{
    return widget->procedure(widget, widget->window, msg);
}

void win_widget_rect(widget_t* widget, rect_t* rect)
{
    *rect = widget->rect;
}

widget_id_t win_widget_id(widget_t* widget)
{
    return widget->id;
}

const char* win_widget_name(widget_t* widget)
{
    return widget->name;
}

void* win_widget_private(widget_t* widget)
{
    return widget->private;
}

void win_widget_private_set(widget_t* widget, void* private)
{
    widget->private = private;
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

void win_theme(win_theme_t* out)
{
    *out = theme;
}

void win_expand_to_window(rect_t* clientArea, dwm_type_t type)
{
    if (type == DWM_WINDOW)
    {
        clientArea->left -= theme.edgeWidth;
        clientArea->top -= theme.edgeWidth + theme.topbarHeight + theme.topbarPadding;
        clientArea->right += theme.edgeWidth;
        clientArea->bottom += theme.edgeWidth;
    }
}

void win_shrink_to_client(rect_t* windowArea, dwm_type_t type)
{
    if (type == DWM_WINDOW)
    {
        windowArea->left += theme.edgeWidth;
        windowArea->top += theme.edgeWidth + theme.topbarHeight + theme.topbarPadding;
        windowArea->right -= theme.edgeWidth;
        windowArea->bottom -= theme.edgeWidth;
    }
}

#endif
