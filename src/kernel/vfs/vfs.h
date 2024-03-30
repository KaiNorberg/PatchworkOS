#pragma once

#include <sys/io.h>

#include "defs/defs.h"
#include "lock/lock.h"

//Note: Only supports ram disks for now.

#define VFS_MAX_NAME_LENGTH 32
#define VFS_MAX_PATH_LENGTH 256

#define FILE_TABLE_LENGTH 64

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
} FileFuncs;

typedef struct Filesystem
{
    char name[VFS_MAX_NAME_LENGTH];
    void* context;
    DiskFuncs diskFuncs;
    FileFuncs fileFuncs;
} Filesystem;

typedef struct Disk
{
    char name[VFS_MAX_NAME_LENGTH];
    void* context;
    DiskFuncs* funcs;
    Filesystem* fs;
} Disk;

typedef struct File
{
    void* context;
    FileFuncs* funcs;
    uint8_t flags;
    _Atomic uint64_t position;
    _Atomic uint64_t ref;
} File;

typedef struct FileTable
{
    File* files[FILE_TABLE_LENGTH];
    Lock lock;
} FileTable;

void file_table_init(FileTable* table);

void file_table_cleanup(FileTable* table);

void vfs_init();

uint64_t vfs_mount(const char* name, void* context, Filesystem* fs);

uint64_t vfs_open(const char* path, uint8_t flags);

uint64_t vfs_close(uint64_t fd);

uint64_t vfs_read(uint64_t fd, void* buffer, uint64_t count);

uint64_t vfs_write(uint64_t fd, const void* buffer, uint64_t count);

uint64_t vfs_seek(uint64_t fd, int64_t offset, uint8_t origin);