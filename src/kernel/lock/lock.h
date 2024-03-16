#pragma once

#include <stdint.h>
#include <stdatomic.h>

typedef struct
{
    _Atomic uint32_t nextTicket;
    _Atomic uint32_t nowServing;
} Lock;

Lock lock_create();

void lock_acquire(Lock* lock);

void lock_release(Lock* lock);