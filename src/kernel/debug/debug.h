#pragma once

#include <stdint.h>

#include "tty/tty.h"
#include "interrupt_frame/interrupt_frame.h"

#define DEBUG_ROW_AMOUNT 20
#define DEBUG_COLUMN_AMOUNT 4

#define DEBUG_COLUMN_WIDTH 25

#define DEBUG_TEXT_SCALE 2

extern const char* exceptionStrings[32];

void debug_panic(const char* message);

void debug_exception(InterruptFrame const* interruptFrame, const char* message);

void debug_move_to_grid(uint8_t row, uint8_t column, Pixel color);

void debug_next_row();