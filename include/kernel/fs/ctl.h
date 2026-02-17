#pragma once

#include <kernel/fs/file.h>
#include <kernel/fs/vnode.h>

#include <stdbool.h>
#include <stdint.h>
#include <sys/status.h>

/**
 * @brief Control file helpers.
 * @defgroup kernel_fs_ctl Control Files
 * @ingroup kernel_fs
 *
 * A control file is a special file that takes in commands as text input and performs actions based on those commands.
 *
 * ## Command Format
 *
 * Commands should be formatted as follows:
 *
 * ```
 * command1 [arguments] [&&|;|\n] command2 [arguments] ...
 * ```
 *
 * The `&&` operator will only execute the next command if the previous command was successful, while the `;` or `\n` operators will always execute the next command.
 *
 * The given command values will be converted from the provided text representation into their `iocmd_t` representation,
 * with the arguments being passed as a string.
 *
 * @{
 */

#define CTL_BUFFER_SIZE 1000 ///< The maximum size of the control buffer. */

/**
 * @brief Control file state structure.
 * @struct ctl_state_t
 */
typedef struct ctl_state
{
    file_t* file;
    char* next;
    uint32_t depth;
    bool runAlways;
    char buffer[CTL_BUFFER_SIZE];
} ctl_state_t;

/**
 * @brief Dispatch control commands from a write IRP.
 *
 * Will parse the buffer of the provided write IRP and send the commands to the provided file.
 *
 * @param irp The IRP containing the control command.
 * @param file The file to dispatch the command to.
 * @return An appropriate status value.
 */
status_t ctl_dispatch(irp_t* irp, file_t* file);

/**
 * @brief A generic write IRP handler for control files.
 *
 * Will simply call `ctl_dispatch()` using the provided IRP.
 *
 * @param irp A write IRP.
 * @return An appropriate status value.
 */
status_t ctl_generic_write(irp_t* irp);

/** @} */
