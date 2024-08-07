#pragma once

#include <errno.h>
#include <stdatomic.h>
#include <sys/list.h>
#include <sys/proc.h>

#include "defs.h"
#include "lock.h"
#include "simd.h"
#include "space.h"
#include "trap.h"
#include "vfs_context.h"

#define THREAD_PRIORITY_LEVELS 3
#define THREAD_PRIORITY_MIN 0
#define THREAD_PRIORITY_MAX (THREAD_PRIORITY_LEVELS - 1)

typedef struct blocker blocker_t;

typedef enum
{
    BLOCK_NORM = 0,
    BLOCK_TIMEOUT = 1
} block_result_t;

typedef struct
{
    list_t list;
    lock_t lock;
} child_storage_t;

typedef struct
{
    pid_t id;
    bool killed;
    char executable[MAX_PATH];
    vfs_context_t vfsContext;
    space_t space;
    child_storage_t children;
    atomic_uint64_t threadCount;
    _Atomic tid_t newTid;
} process_t;

typedef struct
{
    list_entry_t entry;
    process_t* process;
    tid_t id;
    bool killed;
    nsec_t timeStart;
    nsec_t timeEnd;
    nsec_t blockDeadline;
    block_result_t blockResult;
    blocker_t* blocker;
    errno_t error;
    uint8_t priority;
    trap_frame_t trapFrame;
    simd_context_t simdContext;
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} thread_t;

process_t* process_new(const char* executable);

thread_t* thread_new(process_t* process, void* entry, uint8_t priority);

void thread_free(thread_t* thread);

void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

void thread_load(thread_t* thread, trap_frame_t* trapFrame);
