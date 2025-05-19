#pragma once

#include "history.h"
#include "input.h"

#include <libdwm/dwm.h>

#define BLINK_INTERVAL (CLOCKS_PER_SEC / 2)

#define TERMINAL_COLUMNS 80
#define TERMINAL_ROWS 24

#define TERMINAL_WIDTH (TERMINAL_COLUMNS * 8 + windowTheme.edgeWidth * 2 + windowTheme.paddingWidth * 2)
#define TERMINAL_HEIGHT (TERMINAL_ROWS * 16 + windowTheme.edgeWidth * 2 + windowTheme.paddingWidth * 2)

typedef struct
{
    display_t* disp;
    window_t* win;
    font_t* font;
    point_t cursorPos;
    bool cursorVisible;
    fd_t stdin[2];
    fd_t stdout[2]; // Also does stderr
    input_t input;
    history_t history;
    pid_t shell;
} terminal_t;

#define CURSOR_POS_TO_CLIENT_POS(cursorPos, font) \
    (point_t){ \
        .x = ((cursorPos)->x * font_width(font)) + windowTheme.edgeWidth + windowTheme.paddingWidth, \
        .y = ((cursorPos)->y * font_height(font)) + windowTheme.edgeWidth + windowTheme.paddingWidth, \
    };

void terminal_init(terminal_t* term);

void terminal_deinit(terminal_t* term);

bool terminal_update(terminal_t* term);
