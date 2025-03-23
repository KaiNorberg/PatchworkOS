#pragma once

#include <errno.h>
#include <stdatomic.h>
#include <sys/list.h>
#include <sys/proc.h>

#include "config.h"
#include "defs.h"
#include "lock.h"
#include "simd.h"
#include "space.h"
#include "trap.h"
#include "vfs_context.h"

typedef uint8_t priority_t;

#define PRIORITY_LEVELS 3
#define PRIORITY_MIN 0
#define PRIORITY_MAX (PRIORITY_LEVELS - 1)

typedef struct blocker_entry blocker_entry_t;
typedef struct blocker blocker_t;

typedef enum
{
    BLOCK_NORM = 0,
    BLOCK_TIMEOUT = 1,
    BLOCK_ERROR = 2
} block_result_t;

typedef struct
{
    pid_t id;
    char** argv; // Stores both pointers and strings like "[ptr1][ptr2][ptr3][NULL][string1][string2][string3]"
    bool killed;
    vfs_context_t vfsContext;
    space_t space;
    atomic_uint64_t ref;
    _Atomic tid_t newTid;
} process_t;

typedef struct
{
    blocker_entry_t* blockEntires[CONFIG_MAX_BLOCKERS_PER_THREAD];
    uint8_t entryAmount;
    block_result_t result;
    nsec_t deadline;
    atomic_bool blocking;
} block_data_t;

typedef struct
{
    list_entry_t entry;
    process_t* process;
    tid_t id;
    bool killed;
    nsec_t timeStart;
    nsec_t timeEnd;
    block_data_t block;
    errno_t error;
    priority_t priority;
    trap_frame_t trapFrame;
    simd_context_t simdContext;
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} thread_t;

thread_t* thread_new(const char** argv, void* entry, priority_t priority);

void thread_free(thread_t* thread);

thread_t* thread_split(thread_t* thread, void* entry, priority_t priority);

void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

void thread_load(thread_t* thread, trap_frame_t* trapFrame);
