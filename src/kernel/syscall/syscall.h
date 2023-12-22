#pragma once

#include <stdint.h>

#include "page_directory/page_directory.h"
#include "interrupts/interrupts.h"
#include "multitasking/multitasking.h"

void syscall_init();

void syscall_handler(InterruptStackFrame* frame);