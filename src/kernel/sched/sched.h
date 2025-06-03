#pragma once

#include "defs.h"
#include "sched/wait.h"
#include "sync/lock.h"

#include <sys/list.h>

typedef struct process process_t;
typedef struct thread thread_t;

/**
 * @brief The Scheduler.
 * @defgroup
 *
 * The scheduler used in Patchwork features a constant-time algorithm, dynamic priorities, dynamic time slices, basic
 * cpu affinity a focus on reducing lock contention and more.
 */

/**
 * @brief Thread priority type.
 * @ingroup kernel_sched_thread
 * @typedef priority_t
 *
 * The `priority_t` type is used to store the scheduling priority of a thread or process, we also define two constants
 * `PRIORITY_MIN`, which represents the lowest priority a thread can have and `PRIORITY_MAX` which defines the maximum
 * value of a threads priority (not inclusive). See `sched_schedule()` for more info.
 *
 */
typedef uint8_t priority_t;
#define PRIORITY_MAX 64
#define PRIORITY_MIN 0

// TODO: Reimplement load balancing.

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
     * @brief The look that protects sched_cpu_ctx_t::queues, sched_cpu_ctx_t::active and sched_cpu_ctx_t::expired.
     */
    lock_t lock;
    list_t zombieThreads;
    thread_t* idleThread;
    thread_t* runThread;
} sched_cpu_ctx_t;

void sched_thread_ctx_init(sched_thread_ctx_t* ctx);

void sched_cpu_ctx_init(sched_cpu_ctx_t* ctx, cpu_t* cpu);

extern void sched_idle_loop(void);

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

void sched_schedule(trap_frame_t* trapFrame, cpu_t* self);
