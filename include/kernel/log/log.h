#pragma once

#include <kernel/defs.h>

#include <boot/boot_info.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

/**
 * @brief Logging
 * @defgroup kernel_log Logging
 * @ingroup kernel
 *
 * @{
 */

/**
 * @brief Maximum buffer size for a single log line.
 */
#define LOG_MAX_BUFFER 0x1000

/**
 * @brief Logging output options.
 */
typedef enum
{
    LOG_OUTPUT_SERIAL = 1 << 0,
    LOG_OUTPUT_SCREEN = 1 << 1,
    LOG_OUTPUT_FILE = 1 << 2
} log_output_t;

/**
 * @brief Log levels.
 */
typedef enum
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_USER,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERR,
    LOG_LEVEL_PANIC,
} log_level_t;

/**
 * @brief Initialize the logging system.
 *
 * @param gop Pointer to the bootloader-provided GOP information for screen logging.
 */
void log_init(const boot_gop_t* gop);

/**
 * @brief Enable logging to the screen.
 */
void log_screen_enable(void);

/**
 * @brief Disable logging to the screen.
 */
void log_screen_disable(void);

/**
 * @brief Write directly to the log outputs without any formatting or headers.
 *
 * @param string The string to write.
 * @param length The length of the string.
 */
void log_write(const char* string, uint64_t length);

/**
 * @brief Print a formatted log message.
 *
 * @warning If the log level is `LOG_LEVEL_PANIC`, this function will not acquire the log lock to avoid recursive
 * panics. Its up to the panic system to ensure all other CPUs are halted before calling this.
 *
 * @param level The log level.
 * @param format The format string.
 * @param ... The format arguments.
 */
void log_print(log_level_t level, const char* format, ...);

/**
 * @brief Print a formatted log message with a va_list.
 *
 * @warning If the log level is `LOG_LEVEL_PANIC`, this function will not acquire the log lock to avoid recursive
 * panics. Its up to the panic system to ensure all other CPUs are halted before calling this.
 *
 * @param level The log level.
 * @param format The format string.
 * @param args The va_list of arguments.
 */
void log_vprint(log_level_t level, const char* format, va_list args);

#ifndef NDEBUG
#define LOG_DEBUG(format, ...) log_print(LOG_LEVEL_DEBUG, format __VA_OPT__(, ) __VA_ARGS__)
#else
#define LOG_DEBUG(format, ...) ((void)0)
#endif

#define LOG_USER(format, ...) log_print(LOG_LEVEL_USER, format __VA_OPT__(, ) __VA_ARGS__)
#define LOG_INFO(format, ...) log_print(LOG_LEVEL_INFO, format __VA_OPT__(, ) __VA_ARGS__)
#define LOG_WARN(format, ...) log_print(LOG_LEVEL_WARN, format __VA_OPT__(, ) __VA_ARGS__)
#define LOG_ERR(format, ...) log_print(LOG_LEVEL_ERR, format __VA_OPT__(, ) __VA_ARGS__)
#define LOG_PANIC(format, ...) log_print(LOG_LEVEL_PANIC, format __VA_OPT__(, ) __VA_ARGS__)

/** @} */
