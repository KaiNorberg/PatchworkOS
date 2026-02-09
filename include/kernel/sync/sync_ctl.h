#pragma once

#include <kernel/sched/wait.h>

#include <sys/map.h>
#include <sys/proc.h>
#include <sys/sync.h>

/**
 * @brief Kernel-backed user-space synchronization control.
 * @defgroup kernel_sync_sync_ctl User-space Sync Control
 * @ingroup kernel_sync
 *
 * The user-space syncronization control system is inspired by Linux's `futex()` system, allowing user-space programs to
 * perform efficient synchronization by only involving the kernel when absolutely necessary.
 *
 * @see [futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html)
 *
 * @{
 */

/**
 * @brief User-space syncronization object.
 * @struct sync_object_t
 */
typedef struct
{
    map_entry_t entry;
    wait_queue_t queue;
    uintptr_t addr;
} sync_object_t;

/**
 * @brief Per-process syncronization control context.
 * @struct sync_ctl_t
 */
typedef struct
{
    MAP_DEFINE(objects, 16);
    lock_t lock;
} sync_ctl_t;

/**
 * @brief Initialize a per-process syncronization control context.
 *
 * @param ctl Pointer to the control context to initialize.
 */
void sync_ctl_init(sync_ctl_t* ctl);

/**
 * @brief Deinitialize a per-process syncronization control context.
 * *
 * @param ctl Pointer to the control context to deinitialize.
 */
void sync_ctl_deinit(sync_ctl_t* ctl);

/** @} */
