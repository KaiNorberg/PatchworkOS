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

typedef struct Task
{
    RegisterBuffer Registers;

    uint64_t* PageMap;

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

void multitasking_init(VirtualAddressSpace* kernelAddressSpace);

void* task_allocate_memory(Task* task, void* virtualAddress, uint64_t size);

Task* load_next_task();

Task* get_running_task();

Task* get_next_ready_task(Task* task);

Task* create_task(void* entry, VirtualAddressSpace* addressSpace);

void append_task(Task* task);

void erase_task(Task* task);

/*
void yield();

void exit(uint64_t status);
*/