#pragma once

#include <stdint.h>
#include <errno.h>

#include "lock/lock.h"
#include "interrupt_frame/interrupt_frame.h"
#include "vmm/vmm.h"

#define THREAD_STATE_ACTIVE 0
#define THREAD_STATE_KILLED 1
#define THREAD_STATE_BLOCKED 2

#define THREAD_PRIORITY_LEVELS 4
#define THREAD_PRIORITY_MIN 0
#define THREAD_PRIORITY_MAX (THREAD_PRIORITY_LEVELS - 1)

#define THREAD_KERNEL_STACK_SIZE PAGE_SIZE

typedef struct
{
    uint64_t id;
    AddressSpace* addressSpace;
    uint8_t killed;
    _Atomic uint64_t threadCount;
    _Atomic uint64_t newTid;
} Process;

typedef struct Blocker
{
    uintptr_t context;
    uint8_t (*callback)(uintptr_t context);
} Blocker;

typedef struct
{
    Process* process;
    uint64_t id;
    uint64_t timeEnd;
    uint64_t timeStart;
    errno_t errno;
    uint8_t state;
    uint8_t priority;
    uint8_t boost;
    Blocker blocker;
    InterruptFrame interruptFrame;
    uint8_t kernelStack[THREAD_KERNEL_STACK_SIZE];
} Thread;

Process* process_new(void);

Thread* thread_new(Process* process, void* entry, uint8_t priority);

void thread_free(Thread* thread);