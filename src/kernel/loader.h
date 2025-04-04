#pragma once

#include <stdarg.h>

#include "defs.h"
#include "thread.h"

#define LOADER_SPLIT_MAX_ARGS 4

typedef struct
{
    char cwd[MAX_PATH];
} loader_context_t;

extern NORETURN void loader_jump_to_user_space(int argc, char** argv, void* rsp, void* rip);

thread_t* loader_spawn(const char** argv, priority_t priority);

thread_t* loader_split(thread_t* thread, void* entry, priority_t priority, uint64_t argc, va_list args);
