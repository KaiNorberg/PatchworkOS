#pragma once

#include "lock.h"
#include "vfs.h"

typedef struct
{
    char cwd[MAX_PATH];
    File* files[CONFIG_MAX_FILE];
    Lock lock;
} VfsContext;

void vfs_context_init(VfsContext* context);

void vfs_context_cleanup(VfsContext* context);

//Vfs context takes ownership of file reference.
uint64_t vfs_context_open(File* file);

uint64_t vfs_context_close(uint64_t fd);

File* vfs_context_get(uint64_t fd);