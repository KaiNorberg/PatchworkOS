#pragma once

#include "process/process.h"

extern void jump_to_user_space(void* userSpaceFunction, void* stackTop, void* pageDirectory);

void scheduler_visualize();

void scheduler_init();

void scheduler_append(Process* process);

void scheduler_remove(Process* process);

void scheduler_schedule();

Process* scheduler_get_running_process();

void scheduler_yield_to_user_space();