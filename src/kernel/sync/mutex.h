#pragma once

#include "sched/wait.h"
#include "sync/lock.h"

#include <stdbool.h>

typedef struct
{
    wait_queue_t waitQueue;
    bool isAcquired;
    lock_t lock;
} mutex_t;

void mutex_init(mutex_t* mtx);

uint64_t mutex_acquire(mutex_t* mtx);

void mutex_release(mutex_t* mtx);
