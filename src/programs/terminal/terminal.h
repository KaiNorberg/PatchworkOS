#pragma once

#include "history.h"
#include "input.h"

#include <sys/win.h>

#define UMSG_BLINK (UMSG_BASE + 1)

#define BLINK_INTERVAL (SEC / 2)

#define TERMINAL_WIDTH (80 * 8 + winTheme.edgeWidth * 2 + winTheme.padding * 2)
#define TERMINAL_HEIGHT (24 * 16 + winTheme.edgeWidth * 2 + winTheme.padding * 2)

typedef struct
{
    win_t* win;
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
        .x = ((cursorPos)->x * (font)->width) + winTheme.edgeWidth + winTheme.padding, \
        .y = ((cursorPos)->y * (font)->height) + winTheme.edgeWidth + winTheme.padding, \
    };

#define CURSOR_POS_X_OUT_OF_BOUNDS(cursorPosX, font) \
    ((cursorPosX) * (font)->width > TERMINAL_WIDTH - winTheme.edgeWidth * 2 - winTheme.padding * 2)

#define CURSOR_POS_Y_OUT_OF_BOUNDS(cursorPosY, font) \
    ((cursorPosY) * (font)->height > TERMINAL_HEIGHT - winTheme.edgeWidth * 2 - winTheme.padding * 2)

void terminal_init(terminal_t* term);

void terminal_deinit(terminal_t* term);

bool terminal_update(terminal_t* term);
