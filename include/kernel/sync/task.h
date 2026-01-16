#pragma once

#include <kernel/sync/lock.h>

#include <assert.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <time.h>

typedef struct process process_t;

/**
 * @brief Asynchronous Task Primitive
 * @defgroup kernel_sync_task Task
 * @ingroup kernel_sync
 *
 * @{
 */

/**
 * @brief Per-CPU task queues.
 * @struct task_ctx_t
 */
typedef struct task_ctx
{
    list_t timeouts;
    list_t completed;
    lock_t lock;
} task_ctx_t;

/**
 * @brief Task flags.
 * @enum task_flags_t
 */
typedef enum
{
    TASK_DELAYED = 1 << 0, ///< The completion of the task has been delayed.
    TASK_TIMEOUT = 1 << 1, ///< The task is in a timeout queue.
} task_flags_t;

/**
 * @brief Macro to define common task structure members.
 *
 * All tasks contain the below common members:
 * - `list_entry_t entry` - List entry for tasks queues and completion queues.
 * - `list_entry_t timeoutEntry` - List entry for timeout queues.
 * - `task_ctx_t* ctx` - Pointer to the per-CPU task context storing this task for timeouts.
 * - `process_t* process` - Pointer to the process that created the task.
 * - `void* data` - Pointer to user data.
 * - `void (*complete)(_type*)` - Completion callback.
 * - `bool (*cancel)(_type*)` - Cancellation callback, should return `true` if the task was cancelled.
 * - `void (*timeout)(_type*)` - Timeout callback.
 * - `task_flags_t flags` - Task flags.
 * - `errno_t err` - Error code for the task.
 * - `clock_t deadline` - Deadline for the task.
 * - `_resultType result` - Result of the task.
 *
 * @param _type The type of the task structure.
 * @param _resultType The type of the task result.
 */
#define TASK_COMMON(_type, _resultType) \
    list_entry_t entry; \
    list_entry_t timeoutEntry; \
    task_ctx_t* ctx; \
    process_t* process; \
    void* data; \
    void (*complete)(_type*); \
    bool (*cancel)(_type*); \
    void (*timeout)(_type*); \
    task_flags_t flags; \
    errno_t err; \
    clock_t deadline; \
    _resultType result

/**
 * @brief Generic task structure.
 * @struct task_t
 *
 * @warning Due to optimization done while allocating tasks in the async system, no task structure should be larger than
 * this structure.
 */
typedef struct task
{
    TASK_COMMON(struct task, uint64_t);
    uint64_t _padding[4];
} task_t;

/**
 * @brief Task queue structure.
 * @struct task_queue_t
 */
typedef struct
{
    list_t tasks;
} task_queue_t;

/**
 * @brief Initializes a task queue.
 *
 * @param queue Pointer to the task queue to initialize.
 */
static inline void task_queue_init(task_queue_t* queue)
{
    list_init(&queue->tasks);
}

/**
 * @brief Adds a task to the per-CPU timeout queue.
 *
 * @param task Pointer to the task to add.
 */
void task_timeout_add(task_t* task);

/**
 * @brief Removes a task from the per-CPU timeout queue.
 *
 * @param task Pointer to the task to remove.
 */
void task_timeout_remove(task_t* task);

/**
 * @brief Checks for task timeouts on the current CPU and handles them.
 *
 * @warning Must be called with interrupts disabled.
 */
void task_timeouts_check(void);

/**
 * @brief Macro to initialize a tasks common members.
 *
 * @param _task Pointer to the task to initialize.
 */
#define TASK_INIT(_task) \
    ({ \
        (_task)->entry = LIST_ENTRY_CREATE((_task)->entry); \
        (_task)->timeoutEntry = LIST_ENTRY_CREATE((_task)->timeoutEntry); \
        (_task)->ctx = NULL; \
        (_task)->process = NULL; \
        (_task)->data = NULL; \
        (_task)->complete = NULL; \
        (_task)->cancel = NULL; \
        (_task)->timeout = NULL; \
        (_task)->flags = 0; \
        (_task)->err = EOK; \
        (_task)->deadline = CLOCKS_NEVER; \
        (_task)->result = (typeof((_task)->result))0; \
    })

#define TASK_CALL(_task, _func) \
    ({ \
        typeof((_task)->result) result = _func(_task); \
        if ((_task)->err != EOK) \
        { \
            (_task)->flags &= ~TASK_DELAYED; \
            (_task)->complete(_task); \
        } \
        else if (!((_task)->flags & TASK_DELAYED)) \
        { \
            (_task)->result = result; \
            (_task)->complete(_task); \
        } \
        result; \
    })

#define TASK_DELAY_NO_QUEUE(_task) \
    ({ \
        uint64_t result = 0; \
        (_task)->flags |= TASK_DELAYED; \
        if ((_task)->deadline != CLOCKS_NEVER) \
        { \
            if ((_task)->timeout == NULL) \
            { \
                errno = EINVAL; \
                result = ERR; \
            } \
            else \
            { \
                (_task)->flags |= TASK_TIMEOUT; \
                task_timeout_add((task_t*)(_task)); \
            } \
        } \
        result; \
    })

#define TASK_DELAY(_task, _queue) \
    ({ \
        list_push_back(&(_queue)->tasks, &(_task)->entry); \
        uint64_t result = TASK_DELAY_NO_QUEUE(_task); \
        if (result == ERR) \
        { \
            list_remove(&(_task)->entry); \
        } \
        result; \
    })

#define TASK_NEXT(_queue, _type) \
    (list_is_empty(&(_queue)->tasks) ? NULL : CONTAINER_OF(list_first(&(_queue)->tasks), _type, entry))

#define TASK_ERROR(_task, _errno) \
    ({ \
        if ((_task)->flags & TASK_TIMEOUT) \
        { \
            task_timeout_remove((task_t*)(_task)); \
        } \
        list_remove(&(_task)->entry); \
        (_task)->flags &= ~TASK_DELAYED; \
        (_task)->err = (_errno); \
        (_task)->complete((_task)); \
    })

#define TASK_COMPLETE(_task, _result) \
    ({ \
        if ((_task)->flags & TASK_TIMEOUT) \
        { \
            task_timeout_remove((task_t*)(_task)); \
        } \
        list_remove(&(_task)->entry); \
        (_task)->flags &= ~TASK_DELAYED; \
        (_task)->result = (_result); \
        (_task)->complete((_task)); \
    })

#define TASK_CANCEL(_task) \
    ({ \
        uint64_t result = 0; \
        if ((_task)->cancel == NULL) \
        { \
            errno = EINVAL; \
            result = ERR; \
        } \
        else \
        { \
            (_task)->err = ECANCELED; \
            if (!((_task)->cancel(_task))) \
            { \
                (_task)->err = EOK; \
                errno = EBUSY; \
                result = ERR; \
            } \
        } \
        result; \
    })

/** @} */