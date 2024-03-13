#pragma once

#include <stdint.h>

#include <lib-asym.h>

#define VFS_DISK_DELIMITER ':'
#define VFS_NAME_DELIMITER '/'

#define VFS_MAX_NAME_LENGTH 32
#define VFS_MAX_PATH_LENGTH 256

typedef struct File File;

typedef struct Disk
{    
    char name[VFS_MAX_NAME_LENGTH];
    void* internal;

    Status (*open)(struct Disk* disk, File** out, const char* path, uint64_t flags);
    Status (*close)(File* file);
    Status (*read)(File* file, void* buffer, uint64_t length);
    Status (*write)(File* file, const void* buffer, uint64_t length);
    Status (*seek)(File* file, int64_t offset, uint64_t origin);
} Disk;

typedef struct File
{
    Disk* disk;
    void* internal;

    uint64_t position;
    uint64_t flags;
} File;

Disk* disk_new(const char* name, void* internal);

File* file_new(Disk* disk, void* internal, uint64_t flags);

void file_free(File* file);

void vfs_init(void);

Status vfs_mount(Disk* disk);

Status vfs_open(File** out, const char* path, uint64_t flags);

Status vfs_read(File* file, void* buffer, uint64_t length);

Status vfs_write(File* file, const void* buffer, uint64_t length);

Status vfs_close(File* file);

Status vfs_seek(File* file, int64_t offset, uint64_t origin);