#pragma once

#include <kernel/fs/file.h>

#include <stdint.h>

/**
 * @brief Helpers to implement ctl (control) file operations.
 * @defgroup kernel_fs_ctl Ctl
 * @ingroup kernel_fs
 *
 * A ctl file is a special file that takes in commands as text input and performs actions based on those commands.
 *
 * ## Command Format
 *
 * Commands should be formatted as follows:
 *
 * ```
 * command1 arg1 arg2 arg3 ... && command2 arg1 arg2 ... && ...
 * ```
 *
 * @{
 */

/**
 * @brief Helper macro to define a standard ctl write function.
 *
 * This macro defines a write function that dispatches commands to a given array of ctl_t structures.
 *
 * @param name The name of the ctl write function.
 * @param ... The ctl array to dispatch commands to.
 */
#define CTL_STANDARD_WRITE_DEFINE(name, ...) \
    static ctl_t name##ctls[] = __VA_ARGS__; \
    static uint64_t name(file_t* file, const void* buffer, uint64_t count, uint64_t* offset) \
    { \
        (void)offset; \
        return ctl_dispatch(name##ctls, file, buffer, count); \
    }

/**
 * @brief Helper macro to define a standard ctl file operations structure.
 *
 * This macro defines a file operations structure with all standard ctl operations implemented.
 *
 * @param name The name of the ctl file operations structure.
 * @param ... The ctl array to dispatch commands to.
 */
#define CTL_STANDARD_OPS_DEFINE(name, ...) \
    CTL_STANDARD_WRITE_DEFINE(name##write, __VA_ARGS__) \
    static file_ops_t name = (file_ops_t){ \
        .write = name##write, \
    };

/**
 * @brief Type definition for a ctl function.
 */
typedef uint64_t (*ctl_func_t)(file_t* file, uint64_t, const char**);

/**
 * @brief Structure defining a ctl command.
 * @struct ctl_t
 */
typedef struct
{
    const char* name; ///< The name of the command.
    ctl_func_t func;  ///< The function to call for the command.
    uint64_t argcMin; ///< The minimum number of arguments accepted by func.
    uint64_t argcMax; ///< The maximum number of arguments accepted by func.
} ctl_t;

/**
 * @brief Type definition for an array of ctl commands.
 */
typedef ctl_t ctl_array_t[];

/**
 * @brief Dispatch a ctl command.
 *
 * @param ctls The array of ctl commands to dispatch to.
 * @param file The file the ctl command was sent to.
 * @param buffer The buffer containing the command and its arguments.
 * @param count The number of bytes in the buffer.
 * @return On success, the number of bytes processed (count). On failure, `ERR` and `errno` is set.
 */
uint64_t ctl_dispatch(ctl_array_t ctls, file_t* file, const void* buffer, uint64_t count);

/** @} */
