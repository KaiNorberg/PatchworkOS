#pragma once

#include <sys/io.h>

#include "defs.h"
#include "lock.h"

typedef struct file file_t;

typedef struct
{
    char cwd[MAX_PATH];
    file_t* files[CONFIG_MAX_FD];
    lock_t lock;
} vfs_context_t;

void vfs_context_init(vfs_context_t* context);

void vfs_context_deinit(vfs_context_t* context);

fd_t vfs_context_open(vfs_context_t* context, file_t* file);

uint64_t vfs_context_close(vfs_context_t* context, fd_t fd);

fd_t vfs_context_openat(vfs_context_t* context, fd_t fd, file_t* file);

file_t* vfs_context_get(vfs_context_t* context, fd_t fd);
