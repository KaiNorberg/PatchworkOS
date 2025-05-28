#pragma once

#include <stdarg.h>

#include "defs.h"
#include "proc/thread.h"

#define LOADER_THREAD_MAX_ARGS 4

#define LOADER_STACK_ADDRESS(tid) (VMM_LOWER_HALF_MAX - (CONFIG_USER_STACK * ((tid) + 1)) - (PAGE_SIZE * ((tid) + 1)))

typedef struct
{
    char cwd[MAX_PATH];
} loader_ctx_t;

extern NORETURN void loader_jump_to_user_space(int argc, char** argv, void* rsp, void* rip);

thread_t* loader_spawn(const char** argv, priority_t priority, const path_t* cwd);

thread_t* loader_thread_create(thread_t* thread, priority_t priority, void* entry, void* arg);
