#pragma once

#include "sync/lock.h"

typedef lock_t _platform_mutex_t;

#define _PLATFORM_MUTEX_INIT(mutex) lock_init(mutex)
#define _PLATFORM_MUTEX_ACQUIRE(mutex) lock_acquire(mutex)
#define _PLATFORM_MUTEX_RELEASE(mutex) lock_release(mutex)

#define _PLATFORM_HAS_SSE 0
#define _PLATFORM_HAS_IO 0
