#pragma once

#include <stdint.h>
#include <sys/io.h>

//Note: Only supports ram disks for now.

#define VFS_MAX_NAME_LENGTH 32
#define VFS_MAX_PATH_LENGTH 256

typedef struct File File;

typedef struct Disk
{
    char name[VFS_MAX_NAME_LENGTH];
    void* context;
    uint64_t (*open)(struct Disk*, File*, const char*, uint64_t);
} Disk;

typedef struct File
{
    Disk* disk;
    void* context;
    uint64_t position;
    uint64_t flags;
} File;

Disk* disk_new(const char* name, void* context);

void vfs_init();

uint64_t vfs_mount(Disk* disk);

uint64_t vfs_open(const char* path, uint64_t flags);