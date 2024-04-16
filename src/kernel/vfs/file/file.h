#pragma once

#include <stdatomic.h>

#include "defs/defs.h"
#include "heap/heap.h"
#include "vfs/volume/volume.h"

#define FILE_CALL_METHOD(file, method, ...) ((file)->method != NULL ? (file)->method(file __VA_OPT__(,) __VA_ARGS__) : ERROR(EACCES))

typedef struct File File;

typedef struct File
{
    Volume* volume;
    uint64_t position;
    _Atomic(uint64_t) ref;
    void (*cleanup)(File*);
    uint64_t (*read)(File*, void*, uint64_t);
    uint64_t (*write)(File*, const void*, uint64_t);
    uint64_t (*seek)(File*, int64_t, uint8_t);
} File;

void file_init(File* file, Volume* volume);

static inline File* file_ref(File* file)
{
    atomic_fetch_add(&file->ref, 1);
    return file;
}

static inline void file_deref(File* file)
{
    if (atomic_fetch_sub(&file->ref, 1) <= 1)
    {        
        Volume* volume = file->volume;
        if (file->cleanup != NULL)
        {
            file->cleanup(file);
        }
        else
        {
            kfree(file);
        }
        volume_deref(volume);
    }
}