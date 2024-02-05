#pragma once

#include "vector/vector.h"
#include "interrupt_frame/interrupt_frame.h"
#include "page_directory/page_directory.h"

#define PROCESS_STATE_NONE 0
#define PROCESS_STATE_RUNNING 1
#define PROCESS_STATE_READY 2
#define PROCESS_STATE_BLOCKED 3

#define PROCESS_PRIORITY_LEVELS 2
#define PROCESS_PRIORITY_MIN 0
#define PROCESS_PRIORITY_MAX (PROCESS_PRIORITY_LEVELS - 1)

typedef struct
{
    void* physicalAddress;
    void* virtualAddress;
} MemoryBlock;

typedef struct
{    
    uint64_t id;

    PageDirectory* pageDirectory;
    Vector* memoryBlocks;

    InterruptFrame* interruptFrame;

    uint8_t state;
    uint8_t priority;
} Process;

void pid_init();

uint64_t pid_new();

Process* process_new(uint8_t priority);

void* process_allocate_page(Process* process, void* virtualAddress);

void process_free(Process* process);