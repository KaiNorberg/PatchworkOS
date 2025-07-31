#pragma once

#include "argv.h"
#include "fs/sysfs.h"
#include "fs/vfs_ctx.h"
#include "mem/space.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "sync/futex.h"
#include <stdatomic.h>

typedef struct
{
    tid_t newTid;
    list_t aliveThreads;
    list_t zombieThreads;
    lock_t lock;
} process_threads_t;

typedef struct
{
    sysfs_dir_t dir;
    sysfs_file_t prioFile;
    sysfs_file_t cwdFile;
    sysfs_file_t cmdlineFile;
    sysfs_file_t noteFile;
    sysfs_file_t statusFile;
} process_dir_t;

typedef struct process
{
    pid_t id;
    _Atomic(priority_t) priority;
    _Atomic(uint64_t) status;
    argv_t argv;
    vfs_ctx_t vfsCtx;
    space_t space;
    futex_ctx_t futexCtx;
    wait_queue_t dyingWaitQueue;
    atomic_bool isDying;
    process_threads_t threads;
    list_entry_t entry;
    list_t children;
    struct process* parent;
    process_dir_t dir;
} process_t;

process_t* process_new(process_t* parent, const char** argv, const path_t* cwd, priority_t priority);

void process_free(process_t* process);

void process_kill(process_t* process, uint64_t status);

bool process_is_child(process_t* process, pid_t parentId);

void process_kernel_init(void);

void process_procfs_init(void);

process_t* process_get_kernel(void);
