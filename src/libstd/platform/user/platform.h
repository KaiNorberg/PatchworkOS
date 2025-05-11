#pragma once

#ifndef __STDC_NO_THREADS__
#include <threads.h>

typedef mtx_t _PlatformMutex_t;

#define _PLATFORM_MUTEX_INIT(mutex) mtx_init(mutex, mtx_plain)
#define _PLATFORM_MUTEX_DESTROY(mutex) mtx_init(mutex, mtx_plain)
#define _PLATFORM_MUTEX_ACQUIRE(mutex) mtx_lock(mutex)
#define _PLATFORM_MUTEX_RELEASE(mutex) mtx_unlock(mutex)

#else

typedef uint8_t _PlatformMutex_t;

#define _PLATFORM_MUTEX_INIT(mutex)
#define _PLATFORM_MUTEX_DESTROY(mutex)
#define _PLATFORM_MUTEX_ACQUIRE(mutex)
#define _PLATFORM_MUTEX_RELEASE(mutex)

#endif

#define _PLATFORM_HAS_SSE 1
