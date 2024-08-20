#pragma once

#include "defs.h"
#include "trap.h"

#include <bootloader/boot_info.h>

#define LOG_BUFFER_LENGTH 0x1000

#define LOG_BREAK '%'
#define LOG_ADDR 'a'
#define LOG_STR 's'
#define LOG_INT 'd'

#define LOG_ASSERT(condition, msg, ...) \
    ({ \
        if (!(condition)) \
        { \
            log_panic(NULL, __FILE_NAME__ ": " msg __VA_OPT__(, ) __VA_ARGS__); \
        } \
    })

#define LOG_TEST(condition, msg, ...) \
    ({ \
        if (!(condition)) \
        { \
            log_print(msg __VA_OPT__(, ) __VA_ARGS__); \
        } \
    })

void log_init(void);

void log_enable_screen(gop_buffer_t* gopBuffer);

void log_disable_screen(void);

void log_enable_time(void);

void log_print(const char* string, ...);

NORETURN void log_panic(const trap_frame_t* trapFrame, const char* string, ...);
