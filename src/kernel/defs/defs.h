#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "config.h"

#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))

#define ERROR(code) \
({ \
    sched_thread()->error = code; ERR; \
})

#define NULLPTR(code) \
({ \
    sched_thread()->error = code; NULL; \
})
