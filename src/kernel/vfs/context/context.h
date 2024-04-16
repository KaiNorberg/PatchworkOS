#pragma once

#include "lock/lock.h"
#include "vfs/file/file.h"

typedef struct
{
    char workDir[CONFIG_MAX_PATH];
    File* files[CONFIG_FILE_AMOUNT];
    Lock lock;
} VfsContext;

void vfs_context_init(VfsContext* context);

void vfs_context_cleanup(VfsContext* context);

uint64_t vfs_context_open(File* file);

uint64_t vfs_context_close(uint64_t fd);

File* vfs_context_get(uint64_t fd);