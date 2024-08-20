#pragma once

#include "config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ALIGNED(alignment) __attribute__((aligned(alignment)))
#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))
#define NOINLINE __attribute__((noinline))

#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b

#define ERROR(code) \
    ({ \
        sched_thread()->error = code; \
        ERR; \
    })

#define ERRPTR(code) \
    ({ \
        sched_thread()->error = code; \
        NULL; \
    })

typedef _Atomic(uint64_t) atomic_uint64_t;
typedef _Atomic(uint32_t) atomic_uint32_t;
typedef _Atomic(uint16_t) atomic_uint16_t;
typedef _Atomic(uint8_t) atomic_uint8_t;

typedef _Atomic(uint64_t) atomic_int64_t;
typedef _Atomic(uint32_t) atomic_int32_t;
typedef _Atomic(uint16_t) atomic_int16_t;
typedef _Atomic(uint8_t) atomic_int8_t;
