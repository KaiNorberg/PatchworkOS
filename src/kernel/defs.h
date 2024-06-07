#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "config.h"

#define ALIGNED(alignment) __attribute__((aligned(alignment)))
#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))
#define NOINLINE __attribute__((noinline))

#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a ## b

#define ERROR(code) \
({ \
    sched_thread()->error = code; ERR; \
})

#define NULLPTR(code) \
({ \
    sched_thread()->error = code; NULL; \
})
