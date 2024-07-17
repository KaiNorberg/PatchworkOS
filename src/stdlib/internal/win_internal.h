#pragma once

#include <sys/list.h>

typedef struct win win_t;
typedef struct widget widget_t;

#define _WIN_INTERNAL
#include <sys/win.h>

#define WIN_WIDGET_MAX_MSG 8

static win_theme_t theme = {
    .edgeWidth = 3,
    .ridgeWidth = 2,
    .highlight = 0xFFFCFCFC,
    .shadow = 0xFF232629,
    .background = 0xFFBFBFBF,
    .selected = 0xFF00007F,
    .unSelected = 0xFF7F7F7F,
};

typedef struct win
{
    fd_t fd;
    pixel_t* buffer;
    point_t pos;
    uint32_t width;
    uint32_t height;
    rect_t clientArea;
    dwm_type_t type;
    win_proc_t procedure;
    list_t widgets;
    bool selected;
    bool moving;
    char name[DWM_MAX_NAME];
} win_t;

typedef struct widget
{
    list_entry_t base;
    widget_id_t id;
    widget_proc_t procedure;
    rect_t rect;
    win_t* window;
    void* private;
    msg_t messages[WIN_WIDGET_MAX_MSG];
    uint8_t writeIndex;
    uint8_t readIndex;
    char name[DWM_MAX_NAME];
} widget_t;

static inline void win_window_surface(win_t* window, surface_t* surface)
{
    surface->invalidArea = (rect_t){0};
    surface->buffer = window->buffer;
    surface->width = window->width;
    surface->height = window->height;
    surface->stride = surface->width;
}

static inline void win_client_surface(win_t* window, surface_t* surface)
{
    surface->invalidArea = (rect_t){0};
    surface->width = RECT_WIDTH(&window->clientArea);
    surface->height = RECT_HEIGHT(&window->clientArea);
    surface->stride = window->width;
    surface->buffer = (pixel_t*)((uint64_t)window->buffer + (window->clientArea.left * sizeof(pixel_t)) +
        (window->clientArea.top * surface->stride * sizeof(pixel_t)));
}
