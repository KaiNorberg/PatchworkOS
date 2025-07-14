#pragma once

#include "sched/wait.h"
#include "sync/mutex.h"
#include <stdint.h>

#define RWMUTEX_READ_SCOPE(mutex) \
    __attribute__((cleanup(rwmutex_read_cleanup))) rwmutex_t* CONCAT(rm, __COUNTER__) = (mutex); \
    rwmutex_read_acquire((mutex))

#define RWMUTEX_WRITE_SCOPE(mutex) \
    __attribute__((cleanup(rwmutex_write_cleanup))) rwmutex_t* CONCAT(wm, __COUNTER__) = (mutex); \
    rwmutex_write_acquire((mutex))

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

void rwmutex_read_acquire(rwmutex_t* mtx);

uint64_t rwmutex_read_try_acquire(rwmutex_t* mtx);

void rwmutex_read_release(rwmutex_t* mtx);

void rwmutex_write_acquire(rwmutex_t* mtx);

uint64_t rwmutex_write_try_acquire(rwmutex_t* mtx);

void rwmutex_write_release(rwmutex_t* mtx);

static inline void rwmutex_read_cleanup(rwmutex_t** mutex)
{
    rwmutex_read_release(*mutex);
}

static inline void rwmutex_write_cleanup(rwmutex_t** mutex)
{
    rwmutex_write_release(*mutex);
}
