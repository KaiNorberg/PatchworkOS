#pragma once

#include "lock.h"
#include "vfs.h"

typedef struct
{
    char cwd[MAX_PATH];
    file_t* files[CONFIG_MAX_FILE];
    lock_t lock;
} vfs_context_t;

void vfs_context_init(vfs_context_t* context);

void vfs_context_cleanup(vfs_context_t* context);

// Vfs context takes ownership of file reference.
fd_t vfs_context_open(file_t* file);

uint64_t vfs_context_close(fd_t fd);

file_t* vfs_context_get(fd_t fd);
