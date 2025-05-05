#pragma once

#include "history.h"
#include "input.h"

#include <libdwm/dwm.h>

#define UMSG_BLINK (UMSG_BASE + 1)

#define BLINK_INTERVAL (SEC / 2)

#define TERMINAL_WIDTH (80 * 8 + windowTheme.edgeWidth * 2 + windowTheme.paddingWidth * 2)
#define TERMINAL_HEIGHT (24 * 16 + windowTheme.edgeWidth * 2 + windowTheme.paddingWidth * 2)

#define UEVENT_TERMINAL_TIMER (UEVENT_BASE + 1)

typedef struct
{
    display_t* disp;
    window_t* win;
    font_t* font;
    point_t cursorPos;
    bool cursorVisible;
    fd_t stdin[2];
    fd_t stdout[2];
    input_t input;
    history_t history;
    fd_t shellCtl;
} terminal_t;

#define CURSOR_POS_TO_CLIENT_POS(cursorPos, font) \
    (point_t){ \
        .x = ((cursorPos)->x * font_width(font)) + windowTheme.edgeWidth + windowTheme.paddingWidth, \
        .y = ((cursorPos)->y * font_height(font)) + windowTheme.edgeWidth + windowTheme.paddingWidth, \
    };

#define CURSOR_POS_X_OUT_OF_BOUNDS(cursorPosX, font) \
    ((cursorPosX) * (int64_t)font_width(font) > \
        TERMINAL_WIDTH - windowTheme.edgeWidth * 2 - windowTheme.paddingWidth * 2)

#define CURSOR_POS_Y_OUT_OF_BOUNDS(cursorPosY, font) \
    ((cursorPosY) * (int64_t)font_height(font) > \
        TERMINAL_HEIGHT - windowTheme.edgeWidth * 2 - windowTheme.paddingWidth * 2)

void terminal_init(terminal_t* term);

void terminal_deinit(terminal_t* term);

bool terminal_update(terminal_t* term);
