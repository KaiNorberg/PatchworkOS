#pragma once

#include "types/types.h"
#include "tty/tty.h"
#include "interrupt_frame/interrupt_frame.h"

#define DEBUG_ROW_AMOUNT 18
#define DEBUG_COLUMN_AMOUNT 4

#define DEBUG_COLUMN_WIDTH 25

#define DEBUG_TEXT_SCALE 2

extern const char* exceptionStrings[32];

void debug_panic(const char* message);

void debug_exception(InterruptFrame const* interruptFrame, const char* message);