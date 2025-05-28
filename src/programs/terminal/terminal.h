#pragma once

#include "history.h"
#include "input.h"

#include <libpatchwork/patchwork.h>

#define BLINK_INTERVAL (CLOCKS_PER_SEC / 2)

#define TERMINAL_COLUMNS 80
#define TERMINAL_ROWS 24

// TODO: Implement ansi stuff
#define TERMINAL_FOREGROUND ((pixel_t)0xFFFFFFFF)
#define TERMINAL_BACKGROUND ((pixel_t)0xFF000000)

typedef struct
{
    display_t* disp;
    window_t* win;
    font_t* font;
    point_t cursorPos;
    bool isCursorVisible;
    fd_t stdin[2];
    fd_t stdout[2]; // Also does stderr
    input_t input;
    history_t history;
    pid_t shell;
} terminal_t;

void terminal_init(terminal_t* term);

void terminal_deinit(terminal_t* term);

bool terminal_update(terminal_t* term);
