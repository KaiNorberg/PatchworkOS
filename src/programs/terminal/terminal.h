#pragma once

#include <stdbool.h>
#include <stdint.h>

void terminal_init(void);

void terminal_deinit(void);

void terminal_clear(void);

char terminal_input(void);

void terminal_print(const char* str, ...);

void terminal_error(const char* str, ...);
