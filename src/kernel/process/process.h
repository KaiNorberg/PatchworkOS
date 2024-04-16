#pragma once

#include <stdatomic.h>
#include <errno.h>

#include "defs/defs.h"
#include "vmm/vmm.h"
#include "list/list.h"
#include "lock/lock.h"
#include "trap/trap.h"
#include "vfs/vfs.h"
#include "vfs/context/context.h"

#define THREAD_PRIORITY_LEVELS 4
#define THREAD_PRIORITY_MIN 0
#define THREAD_PRIORITY_MAX (THREAD_PRIORITY_LEVELS - 1)

typedef enum
{
    THREAD_STATE_ACTIVE,
    THREAD_STATE_KILLED,
    THREAD_STATE_BLOCKED
} ThreadState;

typedef struct
{
    uint64_t id;
    bool killed;
    char executable[CONFIG_MAX_PATH];
    VfsContext vfsContext;
    Space space;
    _Atomic(uint64_t) threadCount;
    _Atomic(uint64_t) newTid;
} Process;

typedef struct Blocker
{
    uintptr_t context;
    bool (*callback)(uintptr_t context);
} Blocker;

typedef struct
{
    ListEntry base;
    Process* process;
    uint64_t id;
    uint64_t timeStart;
    uint64_t timeEnd;
    uint64_t error;
    uint8_t priority;
    uint8_t boost;
    ThreadState state;
    Blocker blocker;
    TrapFrame trapFrame;
    uint8_t kernelStack[CONFIG_KERNEL_STACK];
} Thread;

Process* process_new(const char* executable);

Thread* thread_new(Process* process, void* entry, uint8_t priority);

void thread_free(Thread* thread);