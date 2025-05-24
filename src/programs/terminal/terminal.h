#pragma once

#include "history.h"
#include "input.h"

#include <libdwm/dwm.h>

#define BLINK_INTERVAL (CLOCKS_PER_SEC / 2)

#define TERMINAL_COLUMNS 80
#define TERMINAL_ROWS 24

#define TERMINAL_WIDTH(font) \
    (TERMINAL_COLUMNS * font_width(font, "a", 1) + windowTheme.edgeWidth * 2 + windowTheme.paddingWidth * 2)
#define TERMINAL_HEIGHT(font) \
    (TERMINAL_ROWS * font_height(font) + windowTheme.edgeWidth * 2 + windowTheme.paddingWidth * 2)

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

// Note: The terminal always uses a mono font so we can use any char for the width
#define CURSOR_POS_TO_CLIENT_POS(cursorPos, font) \
    (point_t){ \
        .x = ((cursorPos)->x * font_width(font, "a", 1)) + windowTheme.edgeWidth + windowTheme.paddingWidth, \
        .y = ((cursorPos)->y * font_height(font)) + windowTheme.edgeWidth + windowTheme.paddingWidth, \
    };

void terminal_init(terminal_t* term);

void terminal_deinit(terminal_t* term);

bool terminal_update(terminal_t* term);
