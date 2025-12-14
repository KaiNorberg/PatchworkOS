#pragma once

#include <kernel/sched/thread.h>

#include <stdarg.h>

/**
 * @brief Program loading and user stack management.
 * @defgroup kernel_sched_loader Program Loader
 * @ingroup kernel_sched
 *
 * The loader is responsible for loading programs into memory, setting up the initial user stack, and performing the
 * jump to userspace.
 *
 * ## Initial User Stack and Registers
 *
 * When a new program is loaded, we pass the command line arguments (argv) to the program via the user stack and
 * registers.
 *
 * The stack is set up as follows:
 *
 * <div align="center">
 * | Stack Contents      |
 * |---------------------|
 * | *argv[argc - 1]     |
 * | ...                 |
 * | *argv[0]            |
 * | NULL                |
 * | argv[argc - 1]      |
 * | ...                 |
 * | argv[0]             |
 * | padding             |
 * </div>
 * 
 * The `argv` pointer is placed in the `rsi` register, and the `argc` value is placed in the `rdi` register.
 *
 * Note that rsp points to argc when the program starts executing.
 *
 * @{
 */

/**
 * @brief Causes the currently running thread to load and execute a new program.
 *
 * Intended to be used as the entry point for a newly created process, causing it to run the specified executable with
 * the given arguments and environment variables.
 *
 * @note This function does not return, instead it transfers execution to the new program in user space, if it fails it
 * will exit the process.
 *
 * @warning The arguments `executable` and `argv` along with their contents will be freed by this function, they must be
 * heap allocated and not used after calling this function.
 *
 * @param executable The path to the executable file, will be freed by the loader.
 * @param argv The argument vector for the new program, will be freed by the loader along with its contents, can be
 * `NULL` if `argc` is `0`.
 * @param argc The number of arguments in `argv`.
 */
_NORETURN void loader_exec(const char* executable, char** argv, uint64_t argc);

/** @} */
