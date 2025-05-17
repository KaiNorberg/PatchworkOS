#pragma once

#include "argv.h"
#include "sync/futex.h"
#include "mem/space.h"
#include "fs/sysfs.h"
#include "fs/vfs_ctx.h"
#include "sched/wait.h"

typedef struct
{
    list_t list;
    lock_t lock;
} process_threads_t;

typedef struct
{
    sysdir_t sysdir;
    sysobj_t ctlObj;
    sysobj_t cwdObj;
    sysobj_t cmdlineObj;
} process_dir_t;

typedef struct process
{
    pid_t id;
    argv_t argv;
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
    process_dir_t dir;
} process_t;

process_t* process_new(process_t* parent, const char** argv);

void process_free(process_t* process);

bool process_is_child(process_t* process, pid_t parentId);

void process_kill(process_t* process);

void process_backend_init(void);
