#pragma once

#include <stdint.h>
#include <stdatomic.h>

typedef struct
{
    _Atomic uint32_t nextTicket;
    _Atomic uint32_t servingTicket;
} Lock;

Lock lock_new();

void lock_acquire(Lock* lock);

void lock_release(Lock* lock);