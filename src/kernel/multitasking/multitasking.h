#pragma once

#include "kernel/kernel/kernel.h"
#include "kernel/virtual_memory/virtual_memory.h"
#include "kernel/heap/heap.h"
#include "kernel/idt/idt.h"

typedef struct
{
    uint64_t RAX, RBX, RCX, RDX, RSP, RBP, RIP, Rflags, CR3;
} TaskRegisters;

typedef struct Task
{
    TaskRegisters Registers;
    struct Task* Next;
    uint8_t State;
} Task;

extern void switch_registers(TaskRegisters* from, TaskRegisters* to);

void multitasking_visualize();

void multitasking_init();

Task* get_next_ready_task(Task* task);

void create_task(void (*main)());

void append_task(Task* task);

void yield();

void exit(uint64_t status);
