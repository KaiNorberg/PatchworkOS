#pragma once

#include <stdint.h>

#include "virtual_memory/virtual_memory.h"
#include "interrupts/interrupts.h"
#include "multitasking/multitasking.h"

void syscall_init();

void syscall_handler(InterruptStackFrame* frame);