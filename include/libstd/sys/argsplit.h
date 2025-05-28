#ifndef _SYS_ARGSPLIT_H
#define _SYS_ARGSPLIT_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/config.h"

/**
 * @brief Standardized argument parsing function.
 *
 * The `argsplit` function parses a input string and splits it into a NULL-terminated array of strings factoring in
 * escape chars spaces and quotation marks, allowing for standardized argument parsing in various parts of the operating
 * system.
 *
 * @param str The input string to be parsed.
 * @param maxLen The maximum length of the input string.
 * @param count A pointer to a `uint64_t` where the number of parsed arguments will be stored.
 * @return On success, returns a NULL-terminated array of strings (the parsed arguments). On failure, returns `NULL`.
 */
const char** argsplit(const char* str, uint64_t maxLen, uint64_t* count);

/**
 * @brief Standardized argument parsing function using a provided buffer.
 *
 * The `argsplit_buf` function is similar to `argsplit` but uses a pre-allocated buffer for storing the parsed
 * arguments, useful for memory management.
 *
 * @param buf A pointer to the buffer to be used for storing the parsed arguments.
 * @param size The size of the provided buffer.
 * @param str The input string to be parsed.
 * @param maxLen The maximum length of the input string.
 * @param count A pointer to a `uint64_t` where the number of parsed arguments will be stored.
 * @return On success, returns a NULL-terminated array of strings (the parsed arguments) stored within the provided
 * buffer. On failure, returns `NULL`.
 */
const char** argsplit_buf(void* buf, uint64_t size, const char* str, uint64_t maxLen, uint64_t* count);

#if defined(__cplusplus)
}
#endif

#endif
