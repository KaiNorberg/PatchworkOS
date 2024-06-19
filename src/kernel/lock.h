#pragma once

#include <stdatomic.h>

#include "defs.h"

typedef struct
{
    _Atomic(uint32_t) nextTicket;
    _Atomic(uint32_t) nowServing;
} lock_t;

#define LOCK_GUARD(lock) \
    __attribute__((cleanup(lock_cleanup))) lock_t* CONCAT(l, __COUNTER__) = (lock); \
    lock_acquire((lock))

void lock_init(lock_t* lock);

void lock_acquire(lock_t* lock);

void lock_release(lock_t* lock);

static inline void lock_cleanup(lock_t** lock)
{
    lock_release(*lock);
}
