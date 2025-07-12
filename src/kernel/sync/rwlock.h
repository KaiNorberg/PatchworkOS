#pragma once

#include <stdatomic.h>

#include "cpu/trap.h"

typedef struct
{
    atomic_uint_fast16_t readTicket;
    atomic_uint_fast16_t readServe;
    atomic_uint_fast16_t writeTicket;
    atomic_uint_fast16_t writeServe;
    atomic_uint_fast16_t activeReaders;
    atomic_bool activeWriter;
} rwlock_t;

#define RWLOCK_READ_SCOPE(lock) \
    __attribute__((cleanup(rwlock_read_cleanup))) rwlock_t* CONCAT(rl, __COUNTER__) = (lock); \
    rwlock_read_acquire((lock))

#define RWLOCK_WRITE_SCOPE(lock) \
    __attribute__((cleanup(rwlock_write_cleanup))) rwlock_t* CONCAT(wl, __COUNTER__) = (lock); \
    rwlock_write_acquire((lock))

void rwlock_init(rwlock_t* lock);

void rwlock_read_acquire(rwlock_t* lock);

void rwlock_read_release(rwlock_t* lock);

void rwlock_write_acquire(rwlock_t* lock);

void rwlock_write_release(rwlock_t* lock);

static inline void rwlock_read_cleanup(rwlock_t** lock)
{
    rwlock_read_release(*lock);
}

static inline void rwlock_write_cleanup(rwlock_t** lock)
{
    rwlock_write_release(*lock);
}
