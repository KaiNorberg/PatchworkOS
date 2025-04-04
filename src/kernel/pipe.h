#pragma once

#include "ring.h"
#include "sched.h"
#include "vfs.h"
#include "waitsys.h"

typedef struct
{
    ring_t ring;
    bool readClosed;
    bool writeClosed;
    wait_queue_t waitQueue;
    lock_t lock;
} pipe_private_t;

typedef struct
{
    file_t* read;
    file_t* write;
} pipe_file_t;

uint64_t pipe_init(pipe_file_t* pipe);
