#pragma once

#include <sys/io.h>

#include "defs/defs.h"
#include "lock/lock.h"

//Note: Only supports ram disks for now.

#define VFS_MAX_NAME_LENGTH 32
#define VFS_MAX_PATH_LENGTH 256

#define VFS_NODE_TYPE_FILE 0
#define VFS_NODE_TYPE_DIRECTORY 1

typedef struct
{

} FileSystem;

typedef struct Inode
{
    char name[VFS_MAX_NAME_LENGTH + 1];
    uint8_t type;
    struct Inode* parent;
    struct Inode* next;
    struct Inode* prev;
    Lock lock;
} Inode;

void vfs_init();

uint64_t vfs_open(const char* path, uint64_t flags);

uint64_t vfs_close(uint64_t fd);