#pragma once

#include <stdint.h>
#include <lib-status.h>

#define VFS_DISK_DELIMITER ':'
#define VFS_NAME_DELIMITER '/'

#define VFS_MAX_NAME_LENGTH 32
#define VFS_MAX_PATH_LENGTH 256

#define VFS_FLAG_CREATE (1 << 0)
#define VFS_FLAG_READ (1 << 1)
#define VFS_FLAG_WRITE (1 << 2)

typedef struct File File;
typedef struct Directory Directory;

typedef struct Disk
{
    void* internal;

    Status (*open)(struct Disk* disk, File** out, const char* path, uint64_t flags);
    Status (*close)(File* file);
} Disk;

typedef struct File
{
    Disk* disk;
    void* internal;

    uint64_t position;
    uint64_t flags;

    Status (*read)(File* file, void* buffer, uint64_t length);
    Status (*write)(File* file, const void* buffer, uint64_t length);
} File;

typedef struct
{
    char name[VFS_MAX_NAME_LENGTH];
    Disk* disk;
} DiskFile;

Disk* disk_new(void* internal);

File* file_new(Disk* disk, void* internal, uint64_t flags);

void file_free(File* file);

void vfs_init();

Status vfs_mount(Disk* disk, const char* name);

Status vfs_open(File** out, const char* path, uint64_t flags);

Status vfs_read(File* file, void* buffer, uint64_t length);

Status vfs_write(File* file, const void* buffer, uint64_t length);

Status vfs_close(File* file);

void vfs_seek(File* file, uint64_t position);