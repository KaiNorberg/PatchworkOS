#pragma once

#include <stdatomic.h>
#include <errno.h>

#include "defs.h"
#include "vmm.h"
#include "list.h"
#include "lock.h"
#include "trap.h"
#include "vfs.h"
#include "pmm.h"
#include "vfs_context.h"

#define THREAD_PRIORITY_LEVELS 4
#define THREAD_PRIORITY_MIN 0
#define THREAD_PRIORITY_MAX (THREAD_PRIORITY_LEVELS - 1)

typedef enum
{
    THREAD_STATE_NONE,
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
    uint8_t boost;
    ThreadState state;
    TrapFrame trapFrame;
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} Thread;

Process* process_new(const char* executable);

Thread* thread_new(Process* process, void* entry, uint8_t priority);

void thread_free(Thread* thread);