#pragma once

#include <threads.h>

typedef mtx_t _PlatformMutex_t;

#define _PLATFORM_MUTEX_INIT(mutex) mtx_init(mutex, mtx_plain)
#define _PLATFORM_MUTEX_ACQUIRE(mutex) mtx_lock(mutex)
#define _PLATFORM_MUTEX_RELEASE(mutex) mtx_unlock(mutex)