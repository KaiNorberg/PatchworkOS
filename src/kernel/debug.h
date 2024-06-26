#pragma once

#include "defs.h"
#include "trap.h"

#define DEBUG_BACKGROUND 0xFF0000AA
#define DEBUG_RED 0xFFFF0000
#define DEBUG_WHITE 0xFFFFFFFF

#define DEBUG_SCALE 2
#define DEBUG_ROW_AMOUNT 18
#define DEBUG_COLUMN_AMOUNT 4
#define DEBUG_COLUMN_WIDTH 25

#include <common/boot_info.h>

#define DEBUG_ASSERT(condition, msg) \
    ({ \
        if (!(condition)) \
        { \
            debug_panic(__FILE_NAME__ ": " msg); \
            while (1) \
            { \
                asm volatile("hlt"); \
            } \
        } \
    })

void debug_init(gop_buffer_t* gopBuffer);

NORETURN void debug_panic(const char* message);

NORETURN void debug_exception(trap_frame_t const* trapFrame, const char* message);
