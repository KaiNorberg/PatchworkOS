#pragma once

#include "defs.h"
#include "sched/wait.h"
#include "sync/lock.h"

#include <sys/list.h>

typedef struct process process_t;
typedef struct thread thread_t;

/**
 * @brief The Scheduler.
 * @defgroup kernel_sched
 *
 * The scheduler used in Patchwork features a constant-time algorithm, tickless design, dynamic priorities, dynamic time
 * slices, etc.
 */

typedef struct
{
    uint64_t length;
    uint64_t bitmap;
    list_t lists[PRIORITY_MAX];
} sched_queues_t;

typedef struct
{
    clock_t timeSlice;
    clock_t deadline;
    priority_t actualPriority;
    clock_t recentBlockTime;
    clock_t prevBlockCheck;
} sched_thread_ctx_t;

typedef struct
{
    sched_queues_t queues[2];
    sched_queues_t* active;
    sched_queues_t* expired;

    /**
     * @brief The currently running thread, accessing the run thread can be a bit weird, if the run thread is accessed
     * by the currently running thread, then there is no need for a lock as it will always se the same value, itself.
     * However it is accessed from another cpu, then the lock is needed.
     */
    thread_t* runThread;
    /**
     * @brief The thread that when the owner cpu is idling. Never changes after boot, so no need for a lock.
     */
    thread_t* idleThread;
    /**
     * @brief The look that protects this context, except the zombieThreads.
     */
    lock_t lock;
    /**
     * @brief Stores threads after they have been killed, used to prevent us from using the kernel stack of a freed
     * thread. Only accessed by the owner cpu, so no need for a lock.
     */
    list_t zombieThreads;
} sched_cpu_ctx_t;

void sched_thread_ctx_init(sched_thread_ctx_t* ctx);

void sched_cpu_ctx_init(sched_cpu_ctx_t* ctx, cpu_t* cpu);

extern void sched_idle_loop(void);

/**
 * @brief Wrapper around `sched_schedule()`.
 * @ingroup kernel_sched
 *
 * The `sched_invoke()` function constructs a trap frame using current cpu state and then calls
 * `sched_schedule()`.
 *
 */
extern void sched_invoke(void);

void sched_init(void);

void sched_done_with_boot_thread(void);

wait_result_t sched_sleep(clock_t timeout);

bool sched_is_idle(void);

thread_t* sched_thread(void);

process_t* sched_process(void);

// The exit functions only mark a thread and/or process for death. So remmember to call sched_invoke or
// sched_schedule after any exit function to give the scheduler a chance to schedule.
void sched_process_exit(uint64_t status);
void sched_thread_exit(void);

void sched_yield(void);

void sched_push(thread_t* thread, thread_t* parent, cpu_t* target);

bool sched_schedule(trap_frame_t* trapFrame, cpu_t* self);
