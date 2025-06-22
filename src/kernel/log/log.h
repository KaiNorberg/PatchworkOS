#pragma once

#include "cpu/trap.h"
#include "defs.h"
#include "fs/sysfs.h"
#include "screen.h"
#include "utils/ring.h"

#include <stdatomic.h>

#define LOG_MAX_BUFFER 0x1000
#define LOG_MAX_TIMESTAMP_BUFFER 32

#define LOG_MAX_STACK_FRAMES 64

#define LOG_NO_PANIC_CPU_ID UINT32_MAX

typedef enum
{
    LOG_OUTPUT_SERIAL = 1 << 0,
    LOG_OUTPUT_SCREEN = 1 << 1,
    LOG_OUTPUT_OBJ = 1 << 2
} log_output_t;

typedef enum
{
    LOG_DEBUG,
    LOG_USER,
    LOG_INFO,
    LOG_WARN,
    LOG_ERR,
    LOG_PANIC,
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
    sysobj_t obj;
} log_obj_t;

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

void log_obj_expose(void);

uint64_t log_print(log_level_t level, const char* format, ...);

uint64_t log_vprint(log_level_t level, const char* format, va_list args);

NORETURN void log_panic(const trap_frame_t* trapFrame, const char* format, ...);
