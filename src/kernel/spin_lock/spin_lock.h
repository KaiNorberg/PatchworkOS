#pragma once

#include <stdatomic.h>

typedef atomic_flag SpinLock;

SpinLock spin_lock_new();

void spin_lock_acquire(SpinLock* lock);

void spin_lock_release(SpinLock* lock);