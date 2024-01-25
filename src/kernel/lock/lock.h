#pragma once

#include <stdatomic.h>

typedef atomic_flag Lock;

void lock_init();

Lock lock_new();

void lock_acquire(Lock* lock);

void lock_release(Lock* lock);