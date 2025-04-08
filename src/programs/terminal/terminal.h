#pragma once

#include <stdbool.h>
#include <stdint.h>

void terminal_init(void);

void terminal_deinit(void);

bool terminal_should_quit(void);

void terminal_clear(void);

void terminal_reset_stdio(void);