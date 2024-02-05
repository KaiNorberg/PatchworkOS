#pragma once

#include "vector/vector.h"
#include "interrupt_frame/interrupt_frame.h"
#include "page_directory/page_directory.h"

#define TASK_STATE_NONE 0
#define TASK_STATE_RUNNING 1
#define TASK_STATE_READY 2
#define TASK_STATE_BLOCKED 3

#define TASK_PRIORITY_LEVELS 2
#define TASK_PRIORITY_MIN 0
#define TASK_PRIORITY_MAX (TASK_PRIORITY_LEVELS - 1)

typedef struct
{
    void* physicalAddress;
    void* virtualAddress;
} MemoryBlock;

typedef struct
{
    PageDirectory* pageDirectory;

    Vector* memoryBlocks;

    uint64_t id;

    uint64_t taskAmount;
} Process;

typedef struct
{
    Process* process;
    InterruptFrame* interruptFrame;

    uint8_t state;
    uint8_t priority;
} Task;

void pid_init();

uint64_t pid_new();

Process* process_new();

void* process_allocate_page(Process* process, void* virtualAddress);

Task* task_new(Process* process, uint8_t priority);

void task_free(Task* task);