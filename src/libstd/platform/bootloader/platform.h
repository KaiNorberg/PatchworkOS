#pragma once

#include <stdint.h>

typedef uint8_t _platform_mutex_t;

#define _PLATFORM_MUTEX_INIT(mutex)
#define _PLATFORM_MUTEX_DESTROY(mutex)
#define _PLATFORM_MUTEX_ACQUIRE(mutex)
#define _PLATFORM_MUTEX_RELEASE(mutex)

#define _PLATFORM_HAS_SSE 0
#define _PLATFORM_HAS_IO 0
