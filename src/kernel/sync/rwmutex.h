#pragma once

#include "sync/lock.h"
#include "sched/wait.h"
#include <stdint.h>

#define RWMUTEX_READ_DEFER(lock) \
    __attribute__((cleanup(rwmutex_read_cleanup))) rwmutex_t* CONCAT(rm, __COUNTER__) = (mutex); \
    rwmutex_read_acquire((lock))

#define RWMUTEX_WRITE_DEFER(lock) \
    __attribute__((cleanup(rwmutex_write_cleanup))) rwmutex_t* CONCAT(wm, __COUNTER__) = (mutex); \
    rwmutex_write_acquire((lock))

typedef struct
{
    uint32_t readers;
    uint32_t writers;
    bool writerActive;
    wait_queue_t readerQueue;
    wait_queue_t writerQueue;
    lock_t lock;
} rwmutex_t;

void rwmutex_init(rwmutex_t* mtx);
