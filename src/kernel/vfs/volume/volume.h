#pragma once

#include <stdatomic.h>

#include "defs/defs.h"
#include "debug/debug.h"
#include "utils/utils.h"

typedef struct Filesystem Filesystem;
typedef struct Volume Volume;
typedef struct File File;

typedef struct Volume
{
    Filesystem* fs;
    _Atomic(uint64_t) ref;
    File* (*open)(Volume*, const char*);
} Volume;

void volume_init(Volume* volume, Filesystem* fs);

static inline Volume* volume_ref(Volume* volume)
{
    atomic_fetch_add(&volume->ref, 1);
    return volume;
}

static inline void volume_deref(Volume* volume)
{
    if (atomic_fetch_sub(&volume->ref, 1) <= 1)
    {
        debug_panic("Volume unmounting not implemented");
    }
}