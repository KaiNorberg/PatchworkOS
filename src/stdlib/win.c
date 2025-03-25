#include "platform/platform.h"
#if _PLATFORM_HAS_WIN

typedef struct win win_t;
typedef struct widget widget_t;

#define _WIN_INTERNAL
#include <sys/win.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/mouse.h>

#define WIN_WIDGET_MAX_MSG 8

typedef struct win
{
    fd_t fd;
    pixel_t* buffer;
    point_t pos;
    uint32_t width;
    uint32_t height;
    rect_t clientRect;
    win_flags_t flags;
    win_proc_t procedure;
    list_t widgets;
    bool selected;
    bool moving;
    bool closeButtonPressed;
    gfx_psf_t* psf;
    nsec_t timerDeadline;
    char name[DWM_MAX_NAME];
} win_t;

typedef struct widget
{
    list_entry_t entry;
    widget_id_t id;
    widget_proc_t procedure;
    rect_t rect;
    win_t* window;
    void* private;
    msg_t messages[WIN_WIDGET_MAX_MSG];
    uint8_t writeIndex;
    uint8_t readIndex;
    char* name;
} widget_t;

// TODO: this should be stored in some sort of config file, lua? make something custom?
#define WIN_DEFAULT_FONT "home:/fonts/zap-vga16.psf"
win_theme_t winTheme = {
    .edgeWidth = 3,
    .rimWidth = 3,
    .ridgeWidth = 2,
    .highlight = 0xFFE0E0E0,
    .shadow = 0xFF6F6F6F,
    .bright = 0xFFFFFFFF,
    .dark = 0xFF000000,
    .background = 0xFFBFBFBF,
    .selected = 0xFF00007F,
    .selectedHighlight = 0xFF2186CD,
    .unSelected = 0xFF7F7F7F,
    .unSelectedHighlight = 0xFFAFAFAF,
    .topbarHeight = 40,
    .padding = 2,
};

static uint64_t win_widget_dispatch(widget_t* widget, const msg_t* msg);
static void win_close_button_draw(win_t* window, gfx_t* gfx);

static uint64_t win_set_rect(win_t* window, const rect_t* rect)
{
    window->pos = (point_t){.x = rect->left, .y = rect->top};
    window->width = RECT_WIDTH(rect);
    window->height = RECT_HEIGHT(rect);

    window->clientRect = RECT_INIT_DIM(0, 0, window->width, window->height);
    win_shrink_to_client(&window->clientRect, window->flags);

    return 0;
}

static inline void win_window_surface(win_t* window, gfx_t* gfx)
{
    gfx->invalidRect = (rect_t){0};
    gfx->buffer = window->buffer;
    gfx->width = window->width;
    gfx->height = window->height;
    gfx->stride = gfx->width;
}

static inline void win_client_surface(win_t* window, gfx_t* gfx)
{
    gfx->invalidRect = (rect_t){0};
    gfx->width = RECT_WIDTH(&window->clientRect);
    gfx->height = RECT_HEIGHT(&window->clientRect);
    gfx->stride = window->width;
    gfx->buffer = &window->buffer[window->clientRect.left + window->clientRect.top * gfx->stride];
}

static void win_topbar_rect(win_t* window, rect_t* rect)
{
    *rect = (rect_t){
        .left = winTheme.edgeWidth + winTheme.padding,
        .top = winTheme.edgeWidth + winTheme.padding,
        .right = window->width - winTheme.edgeWidth - winTheme.padding,
        .bottom = winTheme.topbarHeight + winTheme.edgeWidth - winTheme.padding,
    };
}

static void win_topbar_draw(win_t* window, gfx_t* gfx)
{
    rect_t rect;
    win_topbar_rect(window, &rect);

    gfx_edge(gfx, &rect, winTheme.edgeWidth, winTheme.dark, winTheme.highlight);
    RECT_SHRINK(&rect, winTheme.edgeWidth);
    if (window->selected)
    {
        gfx_gradient(gfx, &rect, winTheme.selected, winTheme.selectedHighlight, GFX_GRADIENT_HORIZONTAL, false);
    }
    else
    {
        gfx_gradient(gfx, &rect, winTheme.unSelected, winTheme.unSelectedHighlight, GFX_GRADIENT_HORIZONTAL, false);
    }

    win_close_button_draw(window, gfx);

    rect.left += winTheme.padding * 3;
    rect.right -= winTheme.topbarHeight;
    gfx_text(gfx, window->psf, &rect, GFX_MIN, GFX_CENTER, 16, window->name, winTheme.background, 0);
}

static void win_close_button_rect(win_t* window, rect_t* rect)
{
    win_topbar_rect(window, rect);
    RECT_SHRINK(rect, winTheme.edgeWidth);
    rect->left = rect->right - (rect->bottom - rect->top);
}

static void win_close_button_draw(win_t* window, gfx_t* gfx)
{
    rect_t rect;
    win_close_button_rect(window, &rect);

    gfx_rim(gfx, &rect, winTheme.rimWidth, winTheme.dark);
    RECT_SHRINK(&rect, winTheme.rimWidth);

    if (window->closeButtonPressed)
    {
        gfx_edge(gfx, &rect, winTheme.edgeWidth, winTheme.shadow, winTheme.highlight);
    }
    else
    {
        gfx_edge(gfx, &rect, winTheme.edgeWidth, winTheme.highlight, winTheme.shadow);
    }
    RECT_SHRINK(&rect, winTheme.edgeWidth);
    gfx_rect(gfx, &rect, winTheme.background);

    RECT_EXPAND(&rect, 32);
    gfx_text(gfx, window->psf, &rect, GFX_CENTER, GFX_CENTER, 32, "x", winTheme.shadow, 0);
}

static void win_background_draw(win_t* window, gfx_t* gfx)
{
    rect_t rect = RECT_INIT_GFX(gfx);

    gfx_rect(gfx, &rect, winTheme.background);
    gfx_edge(gfx, &rect, winTheme.edgeWidth, winTheme.bright, winTheme.dark);
}

static void win_handle_drag_and_close_button(win_t* window, gfx_t* gfx, const msg_mouse_t* data)
{
    rect_t topBar;
    win_topbar_rect(window, &topBar);
    rect_t closeButton;
    win_close_button_rect(window, &closeButton);
    point_t mousePos = data->pos;
    win_screen_to_window(window, &mousePos);

    if (window->moving)
    {
        rect_t rect = RECT_INIT_DIM(window->pos.x + data->delta.x, window->pos.y + data->delta.y, window->width, window->height);
        win_move(window, &rect);

        if (!(data->held & MOUSE_LEFT))
        {
            window->moving = false;
        }
    }
    else if (window->closeButtonPressed)
    {
        if (!RECT_CONTAINS_POINT(&closeButton, &mousePos))
        {
            window->closeButtonPressed = false;
            win_close_button_draw(window, gfx);
        }
        else if (data->released & MOUSE_LEFT)
        {
            win_send(window, LMSG_QUIT, NULL, 0);
        }
    }
    else if (RECT_CONTAINS_POINT(&topBar, &mousePos) && data->pressed & MOUSE_LEFT)
    {
        if (RECT_CONTAINS_POINT(&closeButton, &mousePos))
        {
            window->closeButtonPressed = true;
            win_close_button_draw(window, gfx);
        }
        else
        {
            window->moving = true;
        }
    }
}

static void win_background_procedure(win_t* window, const msg_t* msg)
{
    gfx_t gfx;
    win_window_surface(window, &gfx);

    switch (msg->type)
    {
    case MSG_MOUSE:
    {
        msg_mouse_t* data = (msg_mouse_t*)msg->data;

        if (window->flags & WIN_DECO)
        {
            win_handle_drag_and_close_button(window, &gfx, data);
        }

        wmsg_mouse_t wmsg = *data;
        win_widget_send_all(window, WMSG_MOUSE, &wmsg, sizeof(wmsg_mouse_t));
    }
    break;
    case MSG_KBD:
    {
        msg_kbd_t* data = (msg_kbd_t*)msg->data;

        wmsg_kbd_t wmsg = *data;
        win_widget_send_all(window, WMSG_KBD, &wmsg, sizeof(wmsg_kbd_t));
    }
    break;
    case MSG_SELECT:
    {
        window->selected = true;
        if (window->flags & WIN_DECO)
        {
            win_topbar_draw(window, &gfx);
        }
    }
    break;
    case MSG_DESELECT:
    {
        window->selected = false;
        if (window->flags & WIN_DECO)
        {
            win_topbar_draw(window, &gfx);
        }
    }
    break;
    case LMSG_REDRAW:
    {
        if (window->flags & WIN_DECO)
        {
            win_background_draw(window, &gfx);
            win_topbar_draw(window, &gfx);
        }

        win_widget_send_all(window, WMSG_REDRAW, NULL, 0);
    }
    break;
    }

    if (RECT_AREA(&gfx.invalidRect) != 0 &&
        flush(window->fd, window->buffer, window->width * window->height * sizeof(pixel_t), &gfx.invalidRect) == ERR)
    {
        win_send(window, LMSG_QUIT, NULL, 0);
    }
}

win_t* win_new(const char* name, const rect_t* rect, dwm_type_t type, win_flags_t flags, win_proc_t procedure)
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

    window->fd = open("sys:/dwm");
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
        free(window);
        close(window->fd);
        return NULL;
    }

    window->buffer = calloc(create.width * create.height, sizeof(pixel_t));
    if (window->buffer == NULL)
    {
        free(window);
        close(window->fd);
        return NULL;
    }

    window->flags = flags;
    list_init(&window->widgets);
    window->selected = false;
    window->moving = false;
    window->closeButtonPressed = false;
    window->procedure = procedure;
    strcpy(window->name, name);
    win_set_rect(window, rect);
    window->timerDeadline = NEVER;
    window->psf = gfx_psf_new(WIN_DEFAULT_FONT);
    if (window->psf == NULL)
    {
        free(window);
        free(window->buffer);
        close(window->fd);
        return NULL;
    }

    msg_t msg = {.type = LMSG_INIT, .time = uptime()};
    win_dispatch(window, &msg);

    win_send(window, LMSG_REDRAW, NULL, 0);

    return window;
}

uint64_t win_free(win_t* window)
{
    if (close(window->fd) == ERR)
    {
        return ERR;
    }

    msg_t msg = {.type = LMSG_FREE, .time = uptime()};
    win_dispatch(window, &msg);

    widget_t* temp;
    widget_t* widget;
    LIST_FOR_EACH_SAFE(widget, temp, &window->widgets)
    {
        win_widget_free(widget);
    }

    free(window->buffer);
    free(window);
    return 0;
}

fd_t win_fd(win_t* window)
{
    return window->fd;
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
    nsec_t upTime = uptime();
    nsec_t deadline = timeout != NEVER ? timeout + upTime : NEVER;

    while (true)
    {
        nsec_t nextDeadline = MIN(deadline, window->timerDeadline);
        nsec_t remaining = nextDeadline != NEVER ? (nextDeadline > upTime ? (nextDeadline - upTime) : 0) : NEVER;

        ioctl_window_receive_t receive = {.timeout = remaining};
        if (ioctl(window->fd, IOCTL_WINDOW_RECEIVE, &receive, sizeof(ioctl_window_receive_t)) == ERR)
        {
            return ERR;
        }

        if (receive.outMsg.type != MSG_NONE)
        {
            *msg = receive.outMsg;
            return true;
        }

        upTime = uptime();
        if (window->timerDeadline <= upTime)
        {
            lmsg_timer_t data = {.deadline = window->timerDeadline};
            *msg = (msg_t){.type = LMSG_TIMER, .time = upTime};
            memcpy(msg->data, &data, sizeof(lmsg_timer_t));

            window->timerDeadline = NEVER;
            return true;
        }

        if (deadline < upTime)
        {
            break;
        }
    }

    return false;
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

uint64_t win_draw_begin(win_t* window, gfx_t* gfx)
{
    win_client_surface(window, gfx);
    return 0;
}

uint64_t win_draw_end(win_t* window, gfx_t* gfx)
{
    rect_t rect = (rect_t){
        .left = window->clientRect.left + gfx->invalidRect.left,
        .top = window->clientRect.top + gfx->invalidRect.top,
        .right = window->clientRect.left + gfx->invalidRect.right,
        .bottom = window->clientRect.top + gfx->invalidRect.bottom,
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

    win_set_rect(window, rect);

    return 0;
}

const char* win_name(win_t* window)
{
    return window->name;
}

void win_screen_window_rect(win_t* window, rect_t* rect)
{
    *rect = RECT_INIT_DIM(window->pos.x, window->pos.y, window->width, window->height);
}

void win_screen_client_rect(win_t* window, rect_t* rect)
{
    *rect = (rect_t){
        .left = window->pos.x + window->clientRect.left,
        .top = window->pos.y + window->clientRect.top,
        .right = window->pos.x + window->clientRect.right,
        .bottom = window->pos.y + window->clientRect.bottom,
    };
}

void win_client_rect(win_t* window, rect_t* rect)
{
    *rect = window->clientRect;
}

void win_screen_to_window(win_t* window, point_t* point)
{
    point->x -= window->pos.x;
    point->y -= window->pos.y;
}

void win_screen_to_client(win_t* window, point_t* point)
{
    point->x -= window->pos.x + window->clientRect.left;
    point->y -= window->pos.y + window->clientRect.top;
}

void win_window_to_client(win_t* window, point_t* point)
{
    point->x -= window->clientRect.left;
    point->y -= window->clientRect.top;
}

gfx_psf_t* win_font(win_t* window)
{
    return window->psf;
}

uint64_t win_font_set(win_t* window, const char* path)
{
    gfx_psf_t* psf = gfx_psf_new(path);
    if (psf == NULL)
    {
        return ERR;
    }

    window->psf = psf;
    return 0;
}

widget_t* win_widget(win_t* window, widget_id_t id)
{
    widget_t* widget;
    LIST_FOR_EACH(widget, &window->widgets)
    {
        if (widget->id == id)
        {
            return widget;
        }
        if (widget->id > id)
        {
            return NULL;
        }
    }

    return NULL;
}

uint64_t win_timer_set(win_t* window, nsec_t timeout)
{
    window->timerDeadline = timeout != NEVER ? timeout + uptime() : NEVER;
    return 0;
}

widget_t* win_widget_new(win_t* window, widget_proc_t procedure, const char* name, const rect_t* rect, widget_id_t id)
{
    if (win_widget(window, id) != NULL)
    {
        return NULL;
    }

    widget_t* widget = malloc(sizeof(widget_t));
    list_entry_init(&widget->entry);
    widget->id = id;
    widget->procedure = procedure;
    widget->rect = *rect;
    widget->window = window;
    widget->private = NULL;
    widget->readIndex = 0;
    widget->writeIndex = 0;
    widget->name = malloc(strlen(name) + 1);
    strcpy(widget->name, name);

    msg_t msg = {.type = WMSG_INIT, .time = uptime()};
    win_widget_dispatch(widget, &msg);

    win_widget_send(widget, WMSG_REDRAW, NULL, 0);

    widget_t* other;
    LIST_FOR_EACH(other, &window->widgets)
    {
        if (other->id > widget->id)
        {
            list_prepend(&other->entry, widget);
            return widget;
        }
    }

    list_push(&window->widgets, widget);
    return widget;
}

void win_widget_free(widget_t* widget)
{
    msg_t msg = {.type = WMSG_FREE, .time = uptime()};
    win_widget_dispatch(widget, &msg);

    list_remove(widget);
    free(widget->name);
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

void win_widget_name_set(widget_t* widget, const char* name)
{
    widget->name = realloc(widget->name, strlen(name) + 1);
    strcpy(widget->name, name);

    win_widget_send(widget, WMSG_REDRAW, NULL, 0);
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
    fd_t fd = open("sys:/dwm");
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

void win_expand_to_window(rect_t* clientRect, win_flags_t flags)
{
    if (flags & WIN_DECO)
    {
        clientRect->left -= winTheme.edgeWidth;
        clientRect->top -= winTheme.edgeWidth + winTheme.topbarHeight + winTheme.padding;
        clientRect->right += winTheme.edgeWidth;
        clientRect->bottom += winTheme.edgeWidth;
    }
}

void win_shrink_to_client(rect_t* windowRect, win_flags_t flags)
{
    if (flags & WIN_DECO)
    {
        windowRect->left += winTheme.edgeWidth;
        windowRect->top += winTheme.edgeWidth + winTheme.topbarHeight + winTheme.padding;
        windowRect->right -= winTheme.edgeWidth;
        windowRect->bottom -= winTheme.edgeWidth;
    }
}

#endif