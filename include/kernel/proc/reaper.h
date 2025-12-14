#pragma once

typedef struct process process_t;

/**
 * @brief Process reaper for delayed process freeing.
 * @defgroup kernel_proc_reaper Process reaper
 * 
 * The process reaper ensures that a process is not freed immediately after it is killed. This is important because other processes may still wish to access information about the process, such as its exit status.
 * 
 * The reaper runs periodically and frees any "zombie" processes when they have no remaining threads by deinitializing its `/proc` directory, when none of the proc files or directories of the process are open its resources are finally freed. 
 * 
 * @{
 */

/**
 * @brief Initializes the process reaper.
 */
void reaper_init(void);

/**
 * @brief Pushes a process to be reaped later.
 *
 * @param process The process to be reaped.
 */
void reaper_push(process_t* process);

/** @} */