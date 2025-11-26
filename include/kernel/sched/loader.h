#pragma once

#include <kernel/sched/thread.h>

#include <stdarg.h>

/**
 * @brief Program loading and user stack management.
 * @defgroup kernel_sched_loader Program Loader
 * @ingroup kernel_sched
 *
 * The loader is responsible for loading programs into memory and the jump to userspace.
 *
 * @{
 */

/**
 * @brief Spawns a child process from an executable file.
 *
 * @param argv A NULL-terminated array of strings, where `argv[0]` is the filepath to the desired executable. This array
 * will be pushed to the child stack and the child can find a pointer to this array in its rsi register, along with its
 * length in the rdi register.
 * @param cwd The working directory for the child process, or `NULL` to inherit the parents working directory.
 * @param priority The scheduling priority for the child process, or `PRIORITY_PARENT` to inherit the parent's priority.
 * @param flags Spawn behaviour flags.
 * @return On success, the childs first thread. On failure, `NULL` and `errno` is set.
 */
thread_t* loader_spawn(const char** argv, const path_t* cwd, priority_t priority, spawn_flags_t flags);

/**
 * @brief Creates a new thread within an existing process.
 *
 * @param parent The parent process for the new thread.
 * @param entry The entry point address for the new thread.
 * @param arg An argument to pass to the entry point.
 * @return On success, returns the newly created thread. On failure, returns `NULL` and errno is set.
 */
thread_t* loader_thread_create(process_t* parent, void* entry, void* arg);

/** @} */
