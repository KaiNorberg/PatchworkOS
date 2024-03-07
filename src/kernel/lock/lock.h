#pragma once

#include <stdatomic.h>

typedef struct
{
    atomic_int nextTicket;
    atomic_int servingTicket;
} Lock;

Lock lock_new();

void lock_acquire(Lock* lock);

void lock_release(Lock* lock);