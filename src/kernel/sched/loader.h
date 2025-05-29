#pragma once

#include <stdarg.h>

#include "defs.h"
#include "proc/thread.h"

#define LOADER_USER_STACK_TOP(tid) \
    (VMM_LOWER_HALF_MAX - ((CONFIG_USER_STACK + PAGE_SIZE) * (tid)))

#define LOADER_USER_STACK_BOTTOM(tid) \
    (LOADER_USER_STACK_TOP(tid) - CONFIG_USER_STACK)

#define LOADER_GUARD_PAGE_TOP(tid) \
    (LOADER_USER_STACK_BOTTOM(tid) - 1)

#define LOADER_GUARD_PAGE_BOTTOM(tid) \
    (LOADER_GUARD_PAGE_TOP(tid) - PAGE_SIZE)

typedef struct
{
    char cwd[MAX_PATH];
} loader_ctx_t;

extern NORETURN void loader_jump_to_user_space(int argc, char** argv, void* rsp, void* rip);

thread_t* loader_spawn(const char** argv, priority_t priority, const path_t* cwd);

thread_t* loader_thread_create(thread_t* thread, priority_t priority, void* entry, void* arg);
