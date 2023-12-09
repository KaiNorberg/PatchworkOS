#pragma once

#include "kernel/kernel.h"
#include "virtual_memory/virtual_memory.h"
#include "heap/heap.h"
#include "idt/idt.h"

typedef struct
{
    uint64_t RAX;
    uint64_t RBX;
    uint64_t RCX;
    uint64_t RDX;
    uint64_t RSI;
    uint64_t RDI;
    uint64_t R8;
    uint64_t R9;
    uint64_t R10;
    uint64_t R11;
    uint64_t R12;
    uint64_t R13;
    uint64_t R14;
    uint64_t R15;
    uint64_t RBP;
} RegisterBuffer;

typedef struct TaskMemoryBlock
{
    void* Address;
    uint64_t PageAmount;
    struct TaskMemoryBlock* Next;
} TaskMemoryBlock;

typedef struct Task
{
    RegisterBuffer Registers;

    TaskMemoryBlock* FirstMemoryBlock;
    TaskMemoryBlock* LastMemoryBlock;

    uint64_t StackPointer;
    uint64_t InstructionPointer;
    VirtualAddressSpace* AddressSpace;

    uint64_t StackTop;
    uint64_t StackBottom;

    struct Task* Next;
    struct Task* Prev;
    uint64_t State;
} Task;

void multitasking_visualize();

void multitasking_init();

Task* multitasking_new(void* entry);

void multitasking_free(Task* task);

void multitasking_append(Task* task);

void* task_request_page(Task* task);
void* task_allocate_pages(Task* task, void* virtualAddress, uint64_t pageAmount);

Task* load_next_task();

Task* get_running_task();

Task* get_next_ready_task(Task* task);