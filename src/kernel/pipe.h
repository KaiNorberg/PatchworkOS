#pragma once

#include "ring.h"
#include "sched.h"
#include "vfs.h"
#include "wait.h"

typedef struct
{
    void* buffer;
    ring_t ring;
    bool readClosed;
    bool writeClosed;
    wait_queue_t waitQueue;
    lock_t lock;
    // Note: This pointers are just for checking which end the current file is, they should not be referenced.
    void* readEnd;
    void* writeEnd;
} pipe_private_t;

void pipe_init(void);
