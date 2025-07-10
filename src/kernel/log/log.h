#pragma once

#include "cpu/trap.h"
#include "defs.h"
#include "fs/sysfs.h"
#include "screen.h"
#include "utils/ring.h"

#include <stdatomic.h>

// TODO: Make the logging system common between the bootloader and kernel. Perhaps a shared "logging context" passed via
// bootinfo?

#define LOG_MAX_BUFFER 0x1000
#define LOG_MAX_TIMESTAMP_BUFFER 32

#define LOG_MAX_STACK_FRAMES 64

#define LOG_NO_PANIC_CPU_ID UINT32_MAX

typedef enum
{
    LOG_OUTPUT_SERIAL = 1 << 0,
    LOG_OUTPUT_SCREEN = 1 << 1,
    LOG_OUTPUT_FILE = 1 << 2
} log_output_t;

typedef enum
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_USER,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERR,
    LOG_LEVEL_PANIC,
} log_level_t;

typedef struct
{
    bool isTimeEnabled;
    log_output_t outputs;
    log_level_t minLevel;
} log_config_t;

typedef struct
{
    ring_t ring;
    char buffer[LOG_MAX_BUFFER];
    sysfs_file_t file;
} log_file_t;

typedef struct
{
    char lineBuffer[LOG_MAX_BUFFER];
    char panicBuffer[LOG_MAX_BUFFER];
    char timestampBuffer[LOG_MAX_TIMESTAMP_BUFFER];
    log_config_t config;
    atomic_uint32_t panickingCpuId;
    bool isLastCharNewline;
} log_state_t;

void log_init(void);

void log_enable_time(void);

void log_screen_enable(gop_buffer_t* framebuffer);

void log_disable_screen(void);

screen_t* log_get_screen(void);

log_state_t* log_get_state(void);

void log_file_expose(void);

void log_write(const char* string, uint64_t length);

uint64_t log_print(log_level_t level, const char* format, ...);

uint64_t log_vprint(log_level_t level, const char* format, va_list args);

#define LOG_DEBUG(format, ...) log_print(LOG_LEVEL_DEBUG, format __VA_OPT__(, ) __VA_ARGS__)
#define LOG_INFO(format, ...) log_print(LOG_LEVEL_INFO, format __VA_OPT__(, ) __VA_ARGS__)
#define LOG_WARN(format, ...) log_print(LOG_LEVEL_WARN, format __VA_OPT__(, ) __VA_ARGS__)
#define LOG_ERR(format, ...) log_print(LOG_LEVEL_ERR, format __VA_OPT__(, ) __VA_ARGS__)
