#pragma once

#include <errno.h>

#include "defs/defs.h"
#include "vmm/vmm.h"
#include "lock/lock.h"
#include "trap/trap.h"
#include "vfs/vfs.h"

#define THREAD_PRIORITY_LEVELS 4
#define THREAD_PRIORITY_MIN 0
#define THREAD_PRIORITY_MAX (THREAD_PRIORITY_LEVELS - 1)

#define THREAD_KERNEL_STACK_SIZE (PAGE_SIZE)
#define THREAD_USER_STACK_SIZE (PAGE_SIZE * 4)

typedef enum
{
    THREAD_STATE_ACTIVE,
    THREAD_STATE_KILLED,
    THREAD_STATE_BLOCKED
} ThreadState;

typedef struct
{
    uint64_t id;
    char executable[VFS_MAX_PATH_LENGTH];
    FileTable fileTable;
    Space space;
    bool killed;
    _Atomic uint64_t threadCount;
    _Atomic uint64_t newTid;
} Process;

typedef struct Blocker
{
    uintptr_t context;
    bool (*callback)(uintptr_t context);
} Blocker;

typedef struct
{
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
    uint8_t kernelStack[THREAD_KERNEL_STACK_SIZE];
} Thread;

Process* process_new(const char* executable);

Thread* thread_new(Process* process, void* entry, uint8_t priority);

void thread_free(Thread* thread);