#pragma once

#include "defs.h"
#include "proc/thread.h"
#include "sync/lock.h"

#include <sys/list.h>

typedef struct
{
    uint64_t length;
    list_t list;
    lock_t lock;
} thread_queue_t;

typedef struct
{
    thread_queue_t queues[PRIORITY_LEVELS];
    list_t parkedThreads;
    list_t deadThreads;
    thread_t* runThread;
} sched_ctx_t;

void sched_ctx_init(sched_ctx_t* ctx);

extern void sched_idle_loop(void);

void sched_init(void);

wait_result_t sched_sleep(clock_t timeout);

thread_t* sched_thread(void);

process_t* sched_process(void);

void sched_invoke(void);

void sched_yield(void);

NORETURN void sched_process_exit(uint64_t status);

NORETURN void sched_thread_exit(void);

void sched_push(thread_t* thread);

void sched_timer_trap(trap_frame_t* trapFrame);

void sched_schedule_trap(trap_frame_t* trapFrame);
