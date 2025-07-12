#pragma once

#include "sync/lock.h"
#include "sched/wait.h"
#include <stdint.h>

// We cant use defer macros for mutexes as acquiring one might fail.

typedef struct
{
    uint16_t activeReaders;
    uint16_t waitingWriters;
    bool isWriterActive;
    wait_queue_t readerQueue;
    wait_queue_t writerQueue;
    lock_t lock;
} rwmutex_t;

void rwmutex_init(rwmutex_t* mtx);

void rwmutex_deinit(rwmutex_t* mtx);

uint64_t rwmutex_read_acquire(rwmutex_t* mtx);

uint64_t rwmutex_read_try_acquire(rwmutex_t* mtx);

void rwmutex_read_release(rwmutex_t* mtx);

uint64_t rwmutex_write_acquire(rwmutex_t* mtx);

uint64_t rwmutex_write_try_acquire(rwmutex_t* mtx);

void rwmutex_write_release(rwmutex_t* mtx);
