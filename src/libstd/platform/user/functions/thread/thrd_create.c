#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

typedef struct
{
    thrd_start_t func;
    void* arg;
} _entry_ctx_t;

_THREAD_ENTRY_ATTRIBUTES static void _thread_entry(_thread_t* thread)
{
    _entry_ctx_t* ctx = thread->private;
    thrd_start_t func = ctx->func;
    void* arg = ctx->arg;

    free(ctx);

    thrd_exit(func(arg));
}

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg)
{
    _entry_ctx_t* ctx = malloc(sizeof(_entry_ctx_t));
    if (ctx == NULL)
    {
        return thrd_error;
    }
    ctx->func = func;
    ctx->arg = arg;

    _thread_t* thread = _thread_new(_thread_entry, ctx);
    if (thread == NULL)
    {
        free(ctx);
        return thrd_error;
    }

    thr->id = thread->id;
    return thrd_success;
}
