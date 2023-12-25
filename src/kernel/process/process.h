#pragma once

#include "context/context.h"
#include "page_directory/page_directory.h"

#define PROCESS_STATE_RUNNING 0
#define PROCESS_STATE_READY 1
#define PROCESS_STATE_SLEEPING 2
#define PROCESS_STATE_BLOCKED 3
#define PROCESS_STATE_KILLED 4

typedef struct MemoryBlock
{
    void* physicalAddress;
    void* virtualAddress;
    uint64_t pageAmount;
    struct MemoryBlock* next;
} MemoryBlock;

typedef struct Process
{
    Context* context;

    PageDirectory* pageDirectory;

    MemoryBlock* firstMemoryBlock;
    MemoryBlock* lastMemoryBlock;

    struct Process* next;
    struct Process* prev;
    uint8_t state;
} Process;

Process* process_new(void* entry);

void process_free(Process* process);

void* process_allocate_pages(Process* process, void* virtualAddress, uint64_t pageAmount);
