#pragma once

#include <sys/io.h>

#include "defs/defs.h"
#include "lock/lock.h"
#include "vfs/file_table/file_table.h"

//Note: Only supports ram disks for now.

#define VFS_MAX_NAME_LENGTH 32
#define VFS_MAX_PATH_LENGTH 256

typedef struct Disk
{
    char name[VFS_MAX_NAME_LENGTH];
    void* context;
    fd_t (*open)(struct Disk*, const char*, uint8_t);
    void (*cleanup)(File*);
    uint64_t (*read)(File*, void*, uint64_t);
    uint64_t (*write)(File*, const void*, uint64_t);
    uint64_t (*seek)(File*, int64_t, uint8_t);
} Disk;

Disk* disk_new(const char* name, void* context);

void vfs_init();

uint64_t vfs_mount(Disk* disk);

fd_t vfs_open(const char* path, uint8_t flags);

uint64_t vfs_close(fd_t fd);

uint64_t vfs_read(fd_t fd, void* buffer, uint64_t count);

uint64_t vfs_write(fd_t fd, const void* buffer, uint64_t count);

uint64_t vfs_seek(fd_t fd, int64_t offset, uint8_t origin);