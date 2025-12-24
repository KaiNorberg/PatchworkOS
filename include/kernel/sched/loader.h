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
 * Intended to be used as the entry point for a newly created process, causing it to run the executable specified in its command line arguments.
 *
 * @note This function does not return, instead it transfers execution to the new program in user space, if it fails it
 * will exit the process.
 */
_NORETURN void loader_exec(void);

/** @} */
