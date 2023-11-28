#pragma once

#include "kernel/virtual_memory/virtual_memory.h"

typedef struct ProcessControlBlock
{
    uint64_t StackTop;
    VirtualAddressSpace* AddressSpace;
    struct ProcessControlBlock* NextTask;
    uint64_t State;
} ProcessControlBlock;

void multitasking_init();