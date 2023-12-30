#pragma once

#include "interrupt_frame/interrupt_frame.h"
#include "page_directory/page_directory.h"

#define PROCESS_STATE_RUNNING 0
#define PROCESS_STATE_READY 1
#define PROCESS_STATE_SLEEPING 2
#define PROCESS_STATE_BLOCKED 3
#define PROCESS_STATE_KILLED 4

#define PROCESS_ADDRESS_SPACE_USER_STACK ((void*)(USER_ADDRESS_SPACE_TOP - 0x1000))

typedef struct MemoryBlock
{
    void* physicalAddress;
    void* virtualAddress;
    uint64_t pageAmount;
    struct MemoryBlock* next;
} MemoryBlock;

typedef struct Process
{
    InterruptFrame* interruptFrame;

    PageDirectory* pageDirectory;

    MemoryBlock* firstMemoryBlock;
    MemoryBlock* lastMemoryBlock;

    struct Process* next;
    struct Process* prev;
    uint8_t state;
} Process;

Process* process_new(void* entry); //Creates a process that runs in user space

Process* process_kernel_new(void* entry); //Creates a process that runs in kernel space

void process_free(Process* process);

void* process_allocate_pages(Process* process, void* virtualAddress, uint64_t pageAmount);
