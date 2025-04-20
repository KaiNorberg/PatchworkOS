#pragma once

#include <sys/win.h>

#define UMSG_BLINK (UMSG_BASE + 1)

#define BLINK_INTERVAL (SEC / 2)

#define TERMINAL_WIDTH 750
#define TERMINAL_HEIGHT 500

typedef struct
{
    win_t* win;
    point_t cursorPos;
    bool cursorVisible;
    fd_t stdin[2];
    fd_t stdout[2];
    char inputBuffer[MAX_PATH];
    size_t inputIndex;
    fd_t shellCtl;
} terminal_t;

#define CURSOR_POS_TO_CLIENT_POS(cursorPos, font) \
    (point_t){ \
        .x = ((cursorPos)->x * (font)->width) + winTheme.edgeWidth + winTheme.padding, \
        .y = ((cursorPos)->y * (font)->height) + winTheme.edgeWidth + winTheme.padding, \
    };

void terminal_init(terminal_t* term);

void terminal_deinit(terminal_t* term);

bool terminal_update(terminal_t* term);
