#pragma once

#include <sys/io.h>

#include "defs/defs.h"
#include "lock/lock.h"

//Note: Only supports ram disks for now.

typedef struct Disk Disk;
typedef struct File File;

typedef struct DiskFuncs
{
    uint64_t (*open)(Disk*, File*, const char*);
} DiskFuncs;

typedef struct FileFuncs
{
    void (*cleanup)(File*);
    uint64_t (*read)(File*, void*, uint64_t);
    uint64_t (*write)(File*, const void*, uint64_t);
    uint64_t (*seek)(File*, int64_t, uint8_t);
    uint64_t (*ioctl)(File*, uint64_t, void*);
} FileFuncs;

typedef struct Filesystem
{
    char name[CONFIG_MAX_NAME];
    DiskFuncs diskFuncs;
    FileFuncs fileFuncs;
} Filesystem;

typedef struct Disk
{
    char name[CONFIG_MAX_NAME];
    void* context;
    DiskFuncs* funcs;
    Filesystem* fs;
} Disk;

typedef struct File
{
    void* context;
    FileFuncs* funcs;
    _Atomic uint64_t position;
    _Atomic uint64_t ref;
} File;

typedef struct FileTable
{
    File* files[CONFIG_FILE_AMOUNT];
    Lock lock;
} FileTable;

void file_table_init(FileTable* table);

void file_table_cleanup(FileTable* table);

void vfs_init();

uint64_t vfs_mount(const char* name, void* context, Filesystem* fs);

uint64_t vfs_open(const char* path);

uint64_t vfs_close(uint64_t fd);

uint64_t vfs_read(uint64_t fd, void* buffer, uint64_t count);

uint64_t vfs_write(uint64_t fd, const void* buffer, uint64_t count);

uint64_t vfs_seek(uint64_t fd, int64_t offset, uint8_t origin);