#pragma once

#include "kernel/kernel/kernel.h"
#include "kernel/virtual_memory/virtual_memory.h"
#include "kernel/heap/heap.h"
#include "kernel/idt/idt.h"


typedef struct Task
{
    uint64_t RSP;
    uint64_t CR3;
    uint64_t StackTop;
    uint64_t StackBottom;
    struct Task* Next;
    uint8_t State;
} Task;

extern void switch_task(Task* from, Task* to);

void multitasking_visualize();

void multitasking_init(VirtualAddressSpace* kernelAddressSpace);

Task* get_next_ready_task(Task* task);

void create_task(void (*main)(), VirtualAddressSpace* addressSpace);

void append_task(Task* task);

void yield();

void exit(uint64_t status);
