#pragma once

#include "kernel/kernel/kernel.h"
#include "kernel/virtual_memory/virtual_memory.h"
#include "kernel/heap/heap.h"
#include "kernel/idt/idt.h"

typedef struct
{
    uint64_t R12;
    uint64_t R11;
    uint64_t R10;
    uint64_t R9;
    uint64_t R8;
    uint64_t RBP;
    uint64_t RDI;
    uint64_t RSI;
    uint64_t RBX;
    uint64_t RCX;
    uint64_t RDX;
    uint64_t RAX;
} RegisterBuffer;

typedef struct Task
{
    RegisterBuffer Registers;

    uint64_t StackPointer;
    uint64_t AddressSpace;
    uint64_t InstructionPointer;

    uint64_t StackTop;
    uint64_t StackBottom;

    struct Task* Next;
    uint8_t State;
} Task;

extern void switch_task(Task* from, Task* to);

void multitasking_visualize();

void multitasking_init(VirtualAddressSpace* kernelAddressSpace);

Task* load_next_task();

Task* get_running_task();

Task* get_next_ready_task(Task* task);

void create_task(void (*main)(), VirtualAddressSpace* addressSpace);

void append_task(Task* task);

void yield();

void exit(uint64_t status);
