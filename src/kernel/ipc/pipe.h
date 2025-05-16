#pragma once

#include "utils/ring.h"
#include "sched/sched.h"
#include "fs/vfs.h"
#include "sched/wait.h"

typedef struct
{
    void* buffer;
    ring_t ring;
    bool readClosed;
    bool writeClosed;
    wait_queue_t waitQueue;
    lock_t lock;
    // Note: These pointers are just for checking which end the current file is, they should not be referenced.
    void* readEnd;
    void* writeEnd;
} pipe_private_t;

void pipe_init(void);
