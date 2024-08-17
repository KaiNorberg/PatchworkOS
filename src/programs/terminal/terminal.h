#pragma once

#include <stdbool.h>

void terminal_init(void);

void terminal_cleanup(void);

void terminal_loop(void);

void terminal_clear(void);

void terminal_put(char chr);

void terminal_print(const char* str);

void terminal_error(const char* str);
