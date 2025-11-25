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
 * @param argv A null-terminated array of strings storing the arguments to be passed to usespace and the executable to
 * be loaded in `argv[0]`.
 * @param priority The priority of the first thread within the child process.
 * @param cwd The current working directory for the child process, if `cwd` is null then the child inherits the
 * working directory of the parent.
 * @return On success, returns the main thread of the child process. On failure, returns `NULL` and errno is set.
 */
thread_t* loader_spawn(const char** argv, priority_t priority, const path_t* cwd);

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
