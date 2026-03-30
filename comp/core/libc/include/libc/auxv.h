#ifndef _SYS_AUXV_H
#define _SYS_AUXV_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Auxilary vector for passing information to a dynamic linker.
 * @ingroup libc
 * @defgroup libc_auxv Auxiliary Vector
 *
 * In order to pass additional information to a dynamic linker from the parent process, an array of key-value pairs are
 * pushed to the stack.
 *
 * Each key value pair is represented by a `auxv_t` structure.
 *
 * ## Stack Layout
 *
 * The initial stack layout for a process is structured as follows (growing downwards):
 *
 * | Size                       | Description                   |
 * | :------------------------  | :---------------------------- |
 * | Variable                   | String Data (Arguments, etc.) |
 * | Variable                   | Padding for 16-byte alignment |
 * | `sizeof(auxv_t)`           | AUXV_NULL (End of auxv)       |
 * | `sizeof(auxv_t) * N`       | Auxiliary Vector Entries      |
 * | `sizeof(uintptr_t)`        | NULL (End of argv)            |
 * | `sizeof(uintptr_t) * argc` | Argument Pointers (argv)      |
 * | `sizeof(uintptr_t)`        | Argument Count (argc)         |
 *
 * @note The stack pointer initially points to the argument count.
 *
 * @{
 */

#define AUXV_NULL 0   ///< The end of the array.
#define AUXV_BASE 1   ///< The base address of the interpreter/dynamic linker.
#define AUXV_EXECFD 2 ///< A file descriptor to the executable.

/**
 * @brief Auxiliary vector entry.
 * @struct auxv_t
 */
typedef struct
{
    uint64_t type;  ///< The type of the entry.
    uint64_t value; ///< The value associated with the type.
} auxv_t;

/** @} */

#endif