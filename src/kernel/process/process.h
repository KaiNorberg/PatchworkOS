#pragma once

#include <stdint.h>

#include <lib-asym.h>

#include "lock/lock.h"
#include "vfs/vfs.h"
#include "file_table/file_table.h"
#include "interrupt_frame/interrupt_frame.h"
#include "vmm/vmm.h"

#define PROCESS_STATE_ACTIVE 0
#define PROCESS_STATE_KILLED 1

#define PROCESS_PRIORITY_LEVELS 4
#define PROCESS_PRIORITY_MIN 0
#define PROCESS_PRIORITY_MAX (PROCESS_PRIORITY_LEVELS - 1)

typedef struct
{        
    uint64_t id;

    AddressSpace* addressSpace;
    FileTable* fileTable;

    void* kernelStackTop;
    void* kernelStackBottom;

    uint64_t timeEnd;
    uint64_t timeStart;

    InterruptFrame* interruptFrame;
    Status status;
    
    uint8_t state;
    uint8_t priority;
    uint8_t boost;
} Process;

Process* process_new(void* entry);

void process_free(Process* process);