#pragma once

#include "defs.h"

#include "thread.h"

extern NORETURN void loader_jump_to_user_space(void* rsp, void* rip);

NORETURN void loader_entry(void);

thread_t* loader_spawn(const char** argv, priority_t priority);
