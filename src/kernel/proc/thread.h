#pragma once

#include "config.h"
#include "process.h"
#include "cpu/simd.h"
#include "cpu/trap.h"
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
    THREAD_FRESH,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_DEAD,
    THREAD_PARKED,
    THREAD_PRE_BLOCK,
    THREAD_BLOCKED,
    THREAD_UNBLOCKING,
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
