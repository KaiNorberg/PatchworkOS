#pragma once

#include "defs.h"
#include "trap.h"

#define DEBUG_ROW_AMOUNT 18
#define DEBUG_COLUMN_AMOUNT 4
#define DEBUG_COLUMN_WIDTH 25
#define DEBUG_TEXT_SCALE 2

NORETURN void debug_panic(const char* message);

NORETURN void debug_exception(TrapFrame const* trapFrame, const char* message);