#pragma once

#include <sys/io.h>

#include "defs.h"
#include "vfs.h"
#include "sync/lock.h"

typedef struct file file_t;
typedef struct dir_entry dir_entry_t;

typedef struct
{
    path_t cwd;
    file_t* files[CONFIG_MAX_FD];
    lock_t lock;
} vfs_ctx_t;

void vfs_ctx_init(vfs_ctx_t* ctx, const path_t* cwd);

void vfs_ctx_deinit(vfs_ctx_t* ctx);

file_t* vfs_ctx_get_file(vfs_ctx_t* ctx, fd_t fd);

void vfs_ctx_get_cwd(vfs_ctx_t* ctx, path_t* outCwd);

fd_t vfs_ctx_open(vfs_ctx_t* ctx, file_t* file);

fd_t vfs_ctx_openas(vfs_ctx_t* ctx, fd_t fd, file_t* file);

uint64_t vfs_ctx_close(vfs_ctx_t* ctx, fd_t fd);

fd_t vfs_ctx_dup(vfs_ctx_t* ctx, fd_t oldFd);

fd_t vfs_ctx_dup2(vfs_ctx_t* ctx, fd_t oldFd, fd_t newFd);