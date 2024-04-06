#pragma once

#include <sys/io.h>

#include "defs/defs.h"
#include "lock/lock.h"

//Note: Only supports ram drives for now.

typedef struct Drive Drive;
typedef struct File File;

typedef struct
{
    char* name;
    File* (*open)(Drive*, const char*);
    void (*cleanup)(File*);
    uint64_t (*read)(File*, void*, uint64_t);
    uint64_t (*write)(File*, const void*, uint64_t);
    uint64_t (*seek)(File*, int64_t, uint8_t);
} Filesystem;

typedef struct Drive
{
    void* internal;
    Filesystem* fs;
    _Atomic(uint64_t) ref;
} Drive;

typedef struct File
{
    void* internal;
    Drive* drive;
    _Atomic(uint64_t) position;
    _Atomic(uint64_t) ref;
} File;

typedef struct
{
    char workDir[CONFIG_MAX_PATH];
    File* files[CONFIG_FILE_AMOUNT];
    Lock lock;
} VfsContext;

void vfs_context_init(VfsContext* context);

void vfs_context_cleanup(VfsContext* context);

void vfs_init();

uint64_t vfs_mount(char letter, Filesystem* fs, void* internal);

uint64_t vfs_realpath(char* dest, const char* src);

uint64_t vfs_chdir(const char* path);

uint64_t vfs_open(const char* path);

uint64_t vfs_close(uint64_t fd);

uint64_t vfs_read(uint64_t fd, void* buffer, uint64_t count);

uint64_t vfs_write(uint64_t fd, const void* buffer, uint64_t count);

uint64_t vfs_seek(uint64_t fd, int64_t offset, uint8_t origin);