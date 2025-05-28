#pragma once

#include "cpu/trap.h"
#include "defs.h"

#include <bootloader/boot_info.h>

#define LOG_BUFFER_LENGTH 0x1000

#define LOG_SCROLL_OFFSET 3
#define LOG_MAX_LINE (512)

#define LOG_TEXT_COLOR 0xFFA3A4A3

#define LOG_BREAK '%'
#define LOG_ADDR 'a'
#define LOG_STR 's'
#define LOG_INT 'd'

void log_init(void);

void log_expose(void);

void log_enable_screen(gop_buffer_t* gopBuffer);

void log_disable_screen(void);

void log_enable_time(void);

bool log_is_time_enabled(void);

void log_print(const char* str);

NORETURN void log_panic(const trap_frame_t* trapFrame, const char* string, ...);
