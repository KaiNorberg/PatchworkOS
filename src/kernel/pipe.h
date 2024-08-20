#pragma once

#include "sched.h"
#include "vfs.h"

typedef struct
{
    void* buffer;
    bool readClosed;
    bool writeClosed;
    uint64_t readIndex;
    uint64_t writeIndex;
    blocker_t blocker;
    lock_t lock;
} pipe_private_t;

typedef struct
{
    file_t* read;
    file_t* write;
} pipe_file_t;

uint64_t pipe_init(pipe_file_t* pipe);
