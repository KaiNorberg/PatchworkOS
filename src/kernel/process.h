#pragma once

#include <errno.h>
#include <stdatomic.h>

#include "defs.h"
#include "simd.h"
#include "trap.h"
#include "vfs_context.h"
#include "vmm.h"

#include <sys/list.h>

#define THREAD_PRIORITY_LEVELS 3
#define THREAD_PRIORITY_MIN 0
#define THREAD_PRIORITY_MAX (THREAD_PRIORITY_LEVELS - 1)

typedef enum
{
    THREAD_STATE_NONE,
    THREAD_STATE_PAUSE,
    THREAD_STATE_ACTIVE,
    THREAD_STATE_KILLED
} thread_state_t;

typedef struct
{
    pid_t id;
    bool killed;
    char executable[MAX_PATH];
    vfs_context_t vfsContext;
    space_t space;
    _Atomic(uint64_t) threadCount;
    _Atomic(tid_t) newTid;
} process_t;

typedef struct
{
    list_entry_t base;
    process_t* process;
    tid_t id;
    uint64_t timeStart;
    uint64_t timeEnd;
    errno_t error;
    uint8_t priority;
    thread_state_t state;
    trap_frame_t trapFrame;
    simd_context_t simdContext;
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} thread_t;

process_t* process_new(const char* executable);

thread_t* thread_new(process_t* process, void* entry, uint8_t priority);

void thread_free(thread_t* thread);

void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

void thread_load(thread_t* thread, trap_frame_t* trapFrame);
