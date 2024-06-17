#pragma once

#include <errno.h>
#include <stdatomic.h>

#include "defs.h"
#include "list.h"
#include "simd.h"
#include "trap.h"
#include "vfs_context.h"
#include "vmm.h"

#define THREAD_PRIORITY_LEVELS 3
#define THREAD_PRIORITY_MIN 0
#define THREAD_PRIORITY_MAX (THREAD_PRIORITY_LEVELS - 1)

typedef enum
{
    THREAD_STATE_NONE,
    THREAD_STATE_PAUSE,
    THREAD_STATE_ACTIVE,
    THREAD_STATE_KILLED
} ThreadState;

typedef struct
{
    pid_t id;
    bool killed;
    char executable[MAX_PATH];
    VfsContext vfsContext;
    Space space;
    _Atomic(uint64_t) threadCount;
    _Atomic(tid_t) newTid;
} Process;

typedef struct
{
    ListEntry base;
    Process* process;
    tid_t id;
    uint64_t timeStart;
    uint64_t timeEnd;
    errno_t error;
    uint8_t priority;
    ThreadState state;
    TrapFrame trapFrame;
    SimdContext simdContext;
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} Thread;

Process* process_new(const char* executable);

Thread* thread_new(Process* process, void* entry, uint8_t priority);

void thread_free(Thread* thread);

void thread_save(Thread* thread, const TrapFrame* trapFrame);

void thread_load(Thread* thread, TrapFrame* trapFrame);
