#pragma once

#include <stdint.h>

#include "vfs.h"

#define CTL_STANDARD_WRITE_DEFINE(name, ...) \
    static ctl_t name##ctls[] = __VA_ARGS__; \
    static uint64_t name(file_t* file, const void* buffer, uint64_t count, uint64_t* offset) \
    { \
        return ctl_dispatch(name##ctls, file, buffer, count); \
    }

#define CTL_STANDARD_OPS_DEFINE(name, ...) \
    CTL_STANDARD_WRITE_DEFINE(name##write, __VA_ARGS__) \
    static file_ops_t name = (file_ops_t){ \
        .write = name##write, \
    };

typedef uint64_t (*ctl_func_t)(file_t* file, uint64_t, const char**);

typedef struct
{
    const char* name;
    ctl_func_t func;
    uint64_t argcMin;
    uint64_t argcMax;
} ctl_t;

typedef ctl_t ctl_array_t[];

uint64_t ctl_dispatch(ctl_t* ctls, file_t* file, const void* buffer, uint64_t count);
