#pragma once

#include "sync/lock.h"
#include "sched/wait.h"
#include "fs/sysfs.h"

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct local_listen
{
    list_entry_t entry;
    char address[MAX_NAME];
    list_t backlog;
    uint32_t maxBacklog;
    atomic_uint64_t ref;
    atomic_bool isClosed;
    lock_t lock;
    wait_queue_t waitQueue;
    sysfs_file_t file;
} local_listen_t;

void local_listen_dir_init(void);

local_listen_t* local_listen_new(const char* address, uint32_t maxBacklog);
void local_listen_free(local_listen_t* listen);

local_listen_t* local_listen_ref(local_listen_t* listen);
void local_listen_deref(local_listen_t* listen);
