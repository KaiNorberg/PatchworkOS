#pragma once

#include <stdint.h>

/**
 * @brief Argument Vector
 * @defgroup kernel_proc_argv Argument Vector
 * @ingroup kernel_proc
 *
 * @{
 */

/**
 * @brief Argument Vector structure.
 * @struct argv_t
 *
 * Stores the arguments passed to a process in a contiguous buffer in the format:
 * ```
 * [ptr to arg0][ptr to arg1]...[ptr to argN][NULL][arg0 string][arg1 string]...[argN string]
 * ```
 */
typedef struct
{
    char** buffer;   ///!< Stores both pointers and strings.
    char* empty[1];  ///!< Used to avoid allocations for empty argv
    uint64_t size;   ///!< Size of the buffer in bytes.
    uint64_t amount; ///< Number of arguments (excluding the NULL terminator).
} argv_t;

/**
 * @brief Initializes an argument vector from a source array of strings.
 *
 * @param argv Pointer to the argument vector to initialize.
 * @param src NULL-terminated array of strings to copy into the argument vector. If NULL, initializes an empty argument
 * vector.
 * @return On success, returns 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t argv_init(argv_t* argv, const char** src);

/**
 * @brief Deinitializes an argument vector, freeing any allocated memory.
 *
 * @param argv Pointer to the argument vector to deinitialize.
 */
void argv_deinit(argv_t* argv);

/**
 * @brief Retrieves a pointer to the start of the first string.
 *
 * @param argv Pointer to the argument vector.
 * @param length Output parameter to store the total length of all strings in bytes, will be set to 0 if there are no
 * strings.
 * @return Pointer to the start of the first string or `NULL` if there are no strings.
 */
const char* argv_get_strings(const argv_t* argv, uint64_t* length);

/** @} */
