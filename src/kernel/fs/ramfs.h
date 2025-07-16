#pragma once

#include "fs/dentry.h"
#include "fs/inode.h"
#include "fs/superblock.h"

#include <boot/boot_info.h>

#include <sys/io.h>
#include <sys/list.h>

#define RAMFS_NAME "ramfs"

typedef struct
{
    list_t dentrys; // We store all dentries in here to keep them in memory.
    lock_t lock;
} ramfs_superblock_data_t;

typedef struct
{
    list_entry_t entry;
    dentry_t* dentry;
} ramfs_dentry_data_t;

void ramfs_init(ram_disk_t* disk);
