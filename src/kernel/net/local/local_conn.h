#pragma once

#include "sched/wait.h"
#include "sync/lock.h"
#include "utils/ref.h"
#include "utils/ring.h"

#include <sys/io.h>
#include <sys/list.h>

typedef struct local_listen local_listen_t;

typedef struct local_conn
{
    ref_t ref;
    list_entry_t entry;
    ring_t clientToServer;
    void* clientToServerBuffer;
    ring_t serverToClient;
    void* serverToClientBuffer;
    local_listen_t* listen;
    atomic_bool isClosed;
    lock_t lock;
    wait_queue_t waitQueue;
} local_conn_t;

local_conn_t* local_conn_new(local_listen_t* listen);
void local_conn_free(local_conn_t* conn);
