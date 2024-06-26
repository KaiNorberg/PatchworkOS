#pragma once

#include "defs.h"

#include <common/boot_info.h>

#define LOG_BUFFER_LENGTH 0x1000

#define LOG_PORT 0x3F8

#define LOG_BREAK '%'
#define LOG_ADDR 'a'
#define LOG_STR 's'
#define LOG_INT 'd'

#define LOG_ASSERT(condition, msg, ...) \
    ({ \
        if (!(condition)) \
        { \
            log_panic(msg __VA_OPT__(,) __VA_ARGS__); \
        } \
    })

void log_init(void);

void log_enable_screen(gop_buffer_t* gopBuffer);

void log_disable_screen(void);

void log_enable_time(void);

void log_panic(const char* string, ...);

void log_print(const char* string, ...);
