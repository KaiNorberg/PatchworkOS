#pragma once

#include <stdint.h>

/**
 * @brief Userspace kernel log file.
 * @defgroup kernel_log_file Log file
 * @ingroup kernel_log
 *
 * The kernel logs are exposed to userspace via the readable, writable, and pollable `/dev/klog` file.
 *
 * @{
 */

/**
 * @brief Maximum buffer size for the log file.
 */
#define LOG_FILE_MAX_BUFFER 0x10000

/**
 * @brief Expose the kernel log file to userspace in sysfs.
 */
void log_file_expose(void);

/**
 * @brief Flush the content of the log file to the screen.
 */
void log_file_flush_to_screen(void);

/**
 * @brief Write a string to the kernel log file.
 *
 * @param string The string to write.
 * @param length The length of the string.
 */
void log_file_write(const char* string, uint64_t length);

/** @} */
