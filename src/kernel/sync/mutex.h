#pragma once

#include "sched/wait.h"
#include "sync/lock.h"

#include <stdbool.h>

typedef struct thread thread_t;

#define MUTEX_SCOPE(mutex) \
    __attribute__((cleanup(mutex_cleanup))) mutex_t* CONCAT(m, __COUNTER__) = (mutex); \
    mutex_acquire((mutex))

typedef struct
{
    wait_queue_t waitQueue;
    thread_t* owner;
    lock_t lock;
} mutex_t;

void mutex_init(mutex_t* mtx);

void mutex_acquire(mutex_t* mtx);

void mutex_release(mutex_t* mtx);

static inline void mutex_cleanup(mutex_t** mtx)
{
    mutex_release(*mtx);
}
