#pragma once

#include "argv.h"
#include "futex.h"
#include "space.h"
#include "sysfs.h"
#include "vfs_ctx.h"
#include "wait.h"

typedef struct
{
    list_t list;
    lock_t lock;
} process_threads_t;

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
    _Atomic(tid_t) newTid;
    process_threads_t threads;
    list_entry_t entry;
    list_t children;
    struct process* parent;
} process_t;

process_t* process_new(process_t* parent, const char** argv);

void process_free(process_t* process);

bool process_is_child(process_t* process, pid_t parentId);

void process_kill(process_t* process);

void process_backend_init(void);
