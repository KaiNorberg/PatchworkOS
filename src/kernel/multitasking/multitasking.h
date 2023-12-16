#pragma once

#include "kernel/kernel.h"
#include "virtual_memory/virtual_memory.h"
#include "heap/heap.h"
#include "idt/idt.h"
#include "context/context.h"

typedef struct TaskMemoryBlock
{
    void* Address;
    uint64_t PageAmount;
    struct TaskMemoryBlock* Next;
} TaskMemoryBlock;

typedef struct Task
{
    TaskContext* Context;

    TaskMemoryBlock* FirstMemoryBlock;
    TaskMemoryBlock* LastMemoryBlock;

    struct Task* Next;
    struct Task* Prev;
    uint64_t State;
} Task;

extern void jump_to_user_space(void* userSpaceFunction, void* stackTop, void* addressSpace);

void multitasking_visualize();

void multitasking_init();

Task* multitasking_new(void* entry);

void multitasking_free(Task* task);

void multitasking_schedule();

Task* multitasking_get_running_task();

void multitasking_yield_to_user_space();

void* task_request_page(Task* task);

void* task_allocate_pages(Task* task, void* virtualAddress, uint64_t pageAmount);