#pragma once

#include "fs/vfs.h"

#include <boot/boot_info.h>

#include <sys/list.h>

#define RAMFS_NAME "ramfs"

typedef struct
{
    inode_t inode;
    list_entry_t entry;
    list_t children;
    char name[MAX_NAME];
    uint64_t openedAmount;
    void* data;
} ramfs_inode_t;

void ramfs_init(ram_disk_t* disk);
