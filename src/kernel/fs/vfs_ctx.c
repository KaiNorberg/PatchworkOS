#include "vfs_ctx.h"

#include "sched/sched.h"
#include "utils/log.h"
#include "vfs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

void vfs_ctx_init(vfs_ctx_t* ctx, const path_t* cwd)
{
    if (cwd != NULL && !cwd->isInvalid)
    {
        ctx->cwd = *cwd;
    }
    else
    {
        assert(path_init(&ctx->cwd, "sys:/", NULL) != ERR);
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

    for (uint64_t i = 0; i < CONFIG_MAX_FD; i++)
    {
        if (ctx->files[i] != NULL)
        {
            file_deref(ctx->files[i]);
            ctx->files[i] = NULL;
        }
    }
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

    return ERROR(EMFILE);
}

fd_t vfs_ctx_openas(vfs_ctx_t* ctx, fd_t fd, file_t* file)
{
    LOCK_DEFER(&ctx->lock);

    if (fd >= CONFIG_MAX_FD)
    {
        return ERROR(EINVAL);
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
        return ERROR(EBADF);
    }

    file_deref(ctx->files[fd]);
    ctx->files[fd] = NULL;
    return 0;
}

file_t* vfs_ctx_file(vfs_ctx_t* ctx, fd_t fd)
{
    LOCK_DEFER(&ctx->lock);

    if (fd >= CONFIG_MAX_FD || ctx->files[fd] == NULL)
    {
        return ERRPTR(EBADF);
    }

    return file_ref(ctx->files[fd]);
}

fd_t vfs_ctx_dup(vfs_ctx_t* ctx, fd_t oldFd)
{
    LOCK_DEFER(&ctx->lock);

    if (oldFd >= CONFIG_MAX_FD || ctx->files[oldFd] == NULL)
    {
        return ERROR(EBADF);
    }

    for (fd_t fd = 0; fd < CONFIG_MAX_FD; fd++)
    {
        if (ctx->files[fd] == NULL)
        {
            ctx->files[fd] = file_ref(ctx->files[oldFd]);
            return fd;
        }
    }

    return ERROR(EMFILE);
}

fd_t vfs_ctx_dup2(vfs_ctx_t* ctx, fd_t oldFd, fd_t newFd)
{
    LOCK_DEFER(&ctx->lock);

    if (oldFd >= CONFIG_MAX_FD || newFd >= CONFIG_MAX_FD || ctx->files[oldFd] == NULL)
    {
        return ERROR(EBADF);
    }

    if (ctx->files[newFd] != NULL)
    {
        file_deref(ctx->files[newFd]);
        ctx->files[newFd] = NULL;
    }

    ctx->files[newFd] = file_ref(ctx->files[oldFd]);

    return newFd;
}
