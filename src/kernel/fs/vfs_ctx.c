#include "vfs_ctx.h"

#include "log/log.h"
#include "sched/thread.h"
#include "vfs.h"

#include <assert.h>
#include <string.h>

void vfs_ctx_init(vfs_ctx_t* ctx, const path_t* cwd)
{
    if (cwd == NULL)
    {
        vfs_get_global_root(&ctx->cwd);
    }
    else
    {
        path_copy(&ctx->cwd, cwd);
    }

    for (uint64_t i = 0; i < CONFIG_MAX_FD; i++)
    {
        ctx->files[i] = NULL;
    }
    lock_init(&ctx->lock);
}

void vfs_ctx_deinit(vfs_ctx_t* ctx)
{
    LOCK_DEFER(&ctx->lock);

    path_put(&ctx->cwd);

    for (uint64_t i = 0; i < CONFIG_MAX_FD; i++)
    {
        if (ctx->files[i] != NULL)
        {
            file_deref(ctx->files[i]);
            ctx->files[i] = NULL;
        }
    }
}

file_t* vfs_ctx_get_file(vfs_ctx_t* ctx, fd_t fd)
{
    LOCK_DEFER(&ctx->lock);

    if (fd >= CONFIG_MAX_FD || ctx->files[fd] == NULL)
    {
        errno = EBADF;
        return NULL;
    }

    return file_ref(ctx->files[fd]);
}

void vfs_ctx_get_cwd(vfs_ctx_t* ctx, path_t* outCwd)
{
    LOCK_DEFER(&ctx->lock);

    path_copy(outCwd, &ctx->cwd);
}

uint64_t vfs_ctx_set_cwd(vfs_ctx_t* ctx, const path_t* cwd)
{
    LOCK_DEFER(&ctx->lock);

    path_put(&ctx->cwd);
    path_copy(&ctx->cwd, cwd);

    return 0;
}

fd_t vfs_ctx_open(vfs_ctx_t* ctx, file_t* file)
{
    LOCK_DEFER(&ctx->lock);

    for (fd_t fd = 0; fd < CONFIG_MAX_FD; fd++)
    {
        if (ctx->files[fd] == NULL)
        {
            ctx->files[fd] = file_ref(file);
            return fd;
        }
    }

    errno = EMFILE;
    return ERR;
}

fd_t vfs_ctx_openas(vfs_ctx_t* ctx, fd_t fd, file_t* file)
{
    LOCK_DEFER(&ctx->lock);

    if (fd >= CONFIG_MAX_FD)
    {
        errno = EINVAL;
        return ERR;
    }

    if (ctx->files[fd] != NULL)
    {
        file_deref(ctx->files[fd]);
        ctx->files[fd] = NULL;
    }

    ctx->files[fd] = file_ref(file);
    return fd;
}

uint64_t vfs_ctx_close(vfs_ctx_t* ctx, fd_t fd)
{
    LOCK_DEFER(&ctx->lock);

    if (fd >= CONFIG_MAX_FD || ctx->files[fd] == NULL)
    {
        errno = EBADF;
        return ERR;
    }

    file_deref(ctx->files[fd]);
    ctx->files[fd] = NULL;
    return 0;
}

fd_t vfs_ctx_dup(vfs_ctx_t* ctx, fd_t oldFd)
{
    LOCK_DEFER(&ctx->lock);

    if (oldFd >= CONFIG_MAX_FD || ctx->files[oldFd] == NULL)
    {
        errno = EBADF;
        return ERR;
    }

    for (fd_t fd = 0; fd < CONFIG_MAX_FD; fd++)
    {
        if (ctx->files[fd] == NULL)
        {
            ctx->files[fd] = file_ref(ctx->files[oldFd]);
            return fd;
        }
    }

    errno = EMFILE;
    return ERR;
}

fd_t vfs_ctx_dup2(vfs_ctx_t* ctx, fd_t oldFd, fd_t newFd)
{
    if (oldFd == newFd)
    {
        return newFd;
    }

    LOCK_DEFER(&ctx->lock);

    if (oldFd >= CONFIG_MAX_FD || newFd >= CONFIG_MAX_FD || ctx->files[oldFd] == NULL)
    {
        errno = EBADF;
        return ERR;
    }

    if (ctx->files[newFd] != NULL)
    {
        file_deref(ctx->files[newFd]);
        ctx->files[newFd] = NULL;
    }

    ctx->files[newFd] = file_ref(ctx->files[oldFd]);

    return newFd;
}
