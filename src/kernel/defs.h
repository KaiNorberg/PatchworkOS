#pragma once

#include "config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/atomint.h>

#define ALIGNED(alignment) __attribute__((aligned(alignment)))
#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))
#define NOINLINE __attribute__((noinline))

#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b

#define CONTAINER_OF(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

#define ERROR(code) \
    ({ \
        thread_t* thread = sched_thread(); \
        if (thread != NULL) \
        { \
            thread->error = code; \
        } \
        ERR; \
    })

#define ERRPTR(code) \
    ({ \
        thread_t* thread = sched_thread(); \
        if (thread != NULL) \
        { \
            thread->error = code; \
        } \
        NULL; \
    })
