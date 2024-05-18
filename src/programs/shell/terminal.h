#pragma once

#include <stdint.h>
#include <stdbool.h>

#define TERMINAL_MAX_COMMAND 256
#define TERMINAL_MAX_COLOR 8
#define TERMINAL_BLINK_INTERVAL (SEC / 2)

#define TERMINAL_FOREGROUND 0xFF1D99F3
#define TERMINAL_BACKGROUND 0xFF000000

typedef enum
{
    TERMINAL_STATE_NORMAL,
    TERMINAL_STATE_ESCAPE_1,
    TERMINAL_STATE_ESCAPE_2,
    TERMINAL_STATE_FOREGROUND,
    TERMINAL_STATE_BACKGROUND
} TerminalState;

typedef struct
{
    uint64_t x;
    uint64_t y;
    bool visible;
} Cursor;

void terminal_init();

void terminal_put(const char chr);

void terminal_print(const char* string);

__attribute__((noreturn)) void terminal_loop();
