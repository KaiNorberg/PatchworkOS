#ifndef _SYS_AUXV_H
#define _SYS_AUXV_H 1

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
 * | `sizeof(auxv_t)`           | AT_NULL (End of auxv)       |
 * | `sizeof(auxv_t) * N`       | Auxiliary Vector Entries      |
 * | `sizeof(uintptr_t)`        | NULL (End of argv)            |
 * | `sizeof(uintptr_t)`        | NULL (End of envp)            |
 * | `sizeof(uintptr_t) * envc` | Environment Pointers (envp)   |
 * | `sizeof(uintptr_t) * argc` | Argument Pointers (argv)      |
 * | `sizeof(uintptr_t)`        | Argument Count (argc)         |
 *
 * @note The stack pointer initially points to the argument count.
 *
 * @see https://refspecs.linuxfoundation.org/LSB_1.3.0/IA64/spec/auxiliaryvector.html
 *
 * @{
 */

#define AT_NULL 0   ///< The end of the array.
#define AT_BASE 1   ///< The base address of the interpreter/dynamic linker.
#define AT_EXECFD 2 ///< A file descriptor to the executable.

/**
 * @brief Auxiliary vector entry.
 * @struct auxv_t
 */
typedef struct
{
    long int a_type; ///< Entry type
    union {
        long int a_val;      ///< Integer value
        void* a_ptr;         ///< Pointer value
        void (*a_fcn)(void); ///< Function pointer value
    } a_un;
} auxv_t;

/** @} */

#endif