#pragma once

#include "interrupt_frame/interrupt_frame.h"
#include "page_directory/page_directory.h"

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
    PageDirectory* pageDirectory;

    MemoryBlock* firstMemoryBlock;
    MemoryBlock* lastMemoryBlock;

    uint64_t id;
} Process;

void pid_init();

uint64_t pid_new();

Process* process_new();

void process_free(Process* process);

void* process_allocate_pages(Process* process, void* virtualAddress, uint64_t pageAmount);
