#pragma once

#include "fs/sysfs.h"
#include "sched/wait.h"
#include "sync/lock.h"
#include "utils/map.h"
#include "utils/ref.h"

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct socket_family socket_family_t;

typedef struct local_listen
{
    ref_t ref;
    map_entry_t entry;
    char address[MAX_NAME];
    list_t backlog;
    uint32_t pendingAmount;
    uint32_t maxBacklog;
    bool isClosed;
    lock_t lock;
    wait_queue_t waitQueue;
    sysfs_file_t file;
} local_listen_t;

void local_listen_dir_init(socket_family_t* family);

local_listen_t* local_listen_new(const char* address);
void local_listen_free(local_listen_t* listen);

local_listen_t* local_listen_find(const char* address);
