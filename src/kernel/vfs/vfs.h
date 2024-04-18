#pragma once

#include <sys/io.h>

#include "defs/defs.h"
#include "debug/debug.h"
#include "heap/heap.h"
#include "lock/lock.h"

#define FILE_CALL_METHOD(file, method, ...) ((file)->method != NULL ? (file)->method(file __VA_OPT__(,) __VA_ARGS__) : ERROR(EACCES))

typedef struct Filesystem Filesystem;
typedef struct Volume Volume;
typedef struct File File;

typedef struct Filesystem
{
    char* name;
    uint64_t (*mount)(Volume*);
} Filesystem;

typedef struct Volume
{
    _Atomic(uint64_t) ref;
    Filesystem* fs;
    uint64_t (*unmount)(Volume*);
    uint64_t (*open)(Volume*, File*, const char*);
} Volume;

typedef struct File
{
    _Atomic(uint64_t) ref;
    Volume* volume;
    uint64_t position;
    void (*cleanup)(File*);
    uint64_t (*read)(File*, void*, uint64_t);
    uint64_t (*write)(File*, const void*, uint64_t);
    uint64_t (*seek)(File*, int64_t, uint8_t);
    void* internal;
} File;

File* file_ref(File* file);

void file_deref(File* file);

void vfs_init();

File* vfs_open(const char* path);

uint64_t vfs_mount(char letter, Filesystem* fs);

uint64_t vfs_unmount(char letter);

uint64_t vfs_realpath(char* out, const char* path);

uint64_t vfs_chdir(const char* path);