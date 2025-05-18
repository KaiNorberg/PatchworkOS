#pragma once

#include "config.h"
#include "cpu/simd.h"
#include "cpu/trap.h"
#include "process.h"
#include "sched/wait.h"

#include <errno.h>
#include <sys/list.h>
#include <sys/proc.h>

typedef uint8_t priority_t;

#define PRIORITY_LEVELS 3
#define PRIORITY_MIN 0
#define PRIORITY_MAX (PRIORITY_LEVELS - 1)

typedef enum
{
    THREAD_PARKED, // Is currently doing nothing, not in a queue, not blocking, think of it as "other"
    THREAD_READY, // Is in a schedulers wait queue
    THREAD_RUNNING, // Is currently running
    THREAD_DEAD, // Is waiting to be freed, once this state is set it will never change to anything else
    THREAD_PRE_BLOCK, // Prepearing to block, can be interrupted
    THREAD_BLOCKED, // Is blocking,
    THREAD_UNBLOCKING, // Is unblocking
} thread_state_t;

typedef struct thread
{
    list_entry_t entry;
    process_t* process;
    list_entry_t processEntry;
    tid_t id;
    clock_t timeStart;
    clock_t timeEnd;
    wait_thread_ctx_t wait;
    errno_t error;
    priority_t priority;
    simd_ctx_t simd;
    _Atomic(thread_state_t) state;
    trap_frame_t trapFrame;
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} thread_t;

thread_t* thread_new(process_t* process, void* entry, priority_t priority);

void thread_free(thread_t* thread);

void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

void thread_load(thread_t* thread, trap_frame_t* trapFrame);

bool thread_dead(thread_t* thread);