#pragma once

#include "config.h"
#include "defs.h"
#include "futex.h"
#include "lock.h"
#include "simd.h"
#include "space.h"
#include "sysfs.h"
#include "trap.h"
#include "vfs_ctx.h"
#include "waitsys.h"

#include <errno.h>
#include <stdatomic.h>
#include <sys/list.h>
#include <sys/proc.h>

typedef uint8_t priority_t;

#define PRIORITY_LEVELS 3
#define PRIORITY_MIN 0
#define PRIORITY_MAX (PRIORITY_LEVELS - 1)

typedef struct
{
    char** buffer; // Stores both pointers and strings like "[ptr1][ptr2][ptr3][NULL][string1][string2][string3]"
    uint64_t size;
    uint64_t amount;
} argv_t;

typedef struct
{
    pid_t id;
    argv_t argv;
    sysdir_t* dir;
    vfs_ctx_t vfsCtx;
    space_t space;
    atomic_bool dead;
    atomic_uint64 threadCount;
    wait_queue_t queue;
    futex_ctx_t futexCtx;
    list_t threads;
    _Atomic(tid_t) newTid;
} process_t;

typedef struct thread
{
    list_entry_t entry;
    process_t* process;
    tid_t id;
    bool dead;
    nsec_t timeStart;
    nsec_t timeEnd;
    waitsys_ctx_t waitsys;
    errno_t error;
    priority_t priority;
    simd_ctx_t simd;
    trap_frame_t trapFrame;
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} thread_t;

void process_self_expose(void);

thread_t* thread_new(const char** argv, void* entry, priority_t priority, const path_t* cwd);

thread_t* thread_new_inherit(thread_t* thread, void* entry, priority_t priority);

void thread_free(thread_t* thread);

void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

void thread_load(thread_t* thread, trap_frame_t* trapFrame);
