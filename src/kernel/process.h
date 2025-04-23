#pragma once

#include "futex.h"
#include "space.h"
#include "sysfs.h"
#include "vfs_ctx.h"
#include "waitsys.h"
#include "argv.h"

typedef struct process
{
    pid_t id;
    argv_t argv;
    sysdir_t* dir;
    vfs_ctx_t vfsCtx;
    space_t space;
    atomic_bool dead;
    atomic_uint64 threadCount;
    wait_queue_t queue;
    futex_ctx_t futexCtx;
    list_t threads;
    _Atomic(tid_t) newTid;
} process_t;

process_t* process_new(const char** argv, const path_t* cwd);

void process_free(process_t* process);

void process_self_expose(void);
