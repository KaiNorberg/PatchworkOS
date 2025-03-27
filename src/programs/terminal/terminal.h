#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    TERMINAL_COMMAND,
    TERMINAL_SPAWN
} terminal_state_t;

void terminal_init(void);

void terminal_deinit(void);

void terminal_loop(void);

void terminal_clear(void);

uint64_t terminal_spawn(const char** argv);

void terminal_print(const char* str, ...);

void terminal_error(const char* str, ...);
