#pragma once

#include <kernel/sync/task.h>

/**
 * @brief Kernel Task Implementations
 * @defgroup kernel_sync_tasks Tasks
 * @ingroup kernel_sync
 * 
 * @{
 */

/**
 * @brief No-operation task structure.
 * @struct task_nop_t
 */
typedef struct task_nop
{
    TASK_COMMON(struct task_nop, uint64_t);
} task_nop_t;

bool task_nop_cancel(task_nop_t* task);
void task_nop_timeout(task_nop_t* task);

/** @} */