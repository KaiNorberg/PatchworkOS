#pragma once

#include "defs.h"

#include "thread.h"

extern NORETURN void loader_jump_to_user_space(void* rsp, void* rip);

thread_t* loader_spawn(const char** argv, priority_t priority);

thread_t* loader_split(thread_t* thread, void* entry, priority_t priority);
