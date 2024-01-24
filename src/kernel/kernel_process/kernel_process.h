#pragma once

#include "process/process.h"

#define KERNEL_TASK_BLOCK_VECTOR 0x70

extern void kernel_task_entry(void* entry);
extern void kernel_task_block(uint64_t timeout);

void kernel_process_init();

Task* kernel_task_new(void* entry);

void kernel_task_block_handler(InterruptFrame* interruptFrame);