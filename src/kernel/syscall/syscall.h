#pragma once

#include <stdint.h>

#include "kernel/virtual_memory/virtual_memory.h"
#include "kernel/idt/interrupts/interrupts.h"
#include "kernel/multitasking/multitasking.h"

void syscall_init(VirtualAddressSpace* addressSpace, uint64_t* stack);

uint64_t syscall_handler(RegisterBuffer* registerBuffer, InterruptStackFrame* frame);