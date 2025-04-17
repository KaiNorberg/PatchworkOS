#pragma once

#include "defs.h"
#include "trap.h"

#include <bootloader/boot_info.h>

#define LOG_BUFFER_LENGTH 0x1000

#define LOG_SCROLL_OFFSET 3
#define LOG_MAX_LINE MAX_PATH

#define LOG_BREAK '%'
#define LOG_ADDR 'a'
#define LOG_STR 's'
#define LOG_INT 'd'

#define ASSERT_PANIC_MSG(condition, msg, ...) \
    ({ \
        if (!(condition)) \
        { \
            log_panic(NULL, __FILE__ ": " msg __VA_OPT__(, ) __VA_ARGS__); \
        } \
    })

#define ASSERT_PANIC(condition) \
    ({ \
        if (!(condition)) \
        { \
            log_panic(NULL, __FILE__ ": " #condition); \
        } \
    })

void log_init(void);

void log_expose(void);

void log_enable_screen(gop_buffer_t* gopBuffer);

void log_disable_screen(void);

void log_enable_time(void);

bool log_time_enabled(void);

void log_print(const char* str);

NORETURN void log_panic(const trap_frame_t* trapFrame, const char* string, ...);
