#pragma once

#include "config.h"
#include "cpu/simd.h"
#include "cpu/trap.h"
#include "ipc/note.h"
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
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_ZOMBIE,
    THREAD_PRE_BLOCK,
    THREAD_BLOCKED,
    THREAD_UNBLOCKING,
} thread_state_t;

typedef struct thread
{
    list_entry_t entry;
    process_t* process;
    list_entry_t processEntry; // Used by processes to store threads
    tid_t id;
    clock_t timeStart;
    clock_t timeEnd;
    priority_t priority;
    _Atomic(thread_state_t) state;
    errno_t error;
    wait_thread_ctx_t wait;
    simd_ctx_t simd;
    note_queue_t notes;
    trap_frame_t trapFrame;
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} thread_t;

thread_t* thread_new(process_t* process, void* entry, priority_t priority);

void thread_free(thread_t* thread);

void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

void thread_load(thread_t* thread, trap_frame_t* trapFrame);

bool thread_note_pending(thread_t* thread);

uint64_t thread_send_note(thread_t* thread, const void* message, uint64_t length);