#pragma once

#include "kernel/kernel.h"
#include "page_directory/page_directory.h"
#include "heap/heap.h"
#include "idt/idt.h"
#include "context/context.h"
#include "interrupts/interrupts.h"

#define TASK_STATE_RUNNING 0
#define TASK_STATE_READY 1
#define TASK_STATE_WAITING 2

typedef struct MemoryBlock
{
    void* physicalAddress;
    void* virtualAddress;
    uint64_t pageAmount;
    struct MemoryBlock* next;
} MemoryBlock;

typedef struct Task
{
    Context* context;

    PageDirectory* pageDirectory;

    MemoryBlock* firstMemoryBlock;
    MemoryBlock* lastMemoryBlock;
    
    struct Task* next;
    struct Task* prev;
    uint64_t state;
} Task;

extern void jump_to_user_space(void* userSpaceFunction, void* stackTop, void* pageDirectory);

void multitasking_visualize();

void multitasking_init();

Task* multitasking_new(void* entry);

void multitasking_free(Task* task);

void multitasking_schedule();

Task* multitasking_get_running_task();

void multitasking_yield_to_user_space();

void* task_request_page(Task* task, void* virtualAddress);

void* task_allocate_pages(Task* task, void* virtualAddress, uint64_t pageAmount);