#pragma once

#include <stdint.h>

#include "virtual_memory/virtual_memory.h"
#include "idt/interrupts/interrupts.h"
#include "multitasking/multitasking.h"

void syscall_init(VirtualAddressSpace* addressSpace);

uint64_t syscall_handler(RegisterBuffer* registerBuffer, InterruptStackFrame* frame);