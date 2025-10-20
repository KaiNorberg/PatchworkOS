#include "vfs_ctx.h"

#include "log/log.h"
#include "sched/thread.h"
#include "vfs.h"

#include <assert.h>
#include <string.h>
#include <sys/io.h>

void vfs_ctx_init(vfs_ctx_t* ctx, const path_t* cwd)
{
    ctx->cwd = PATH_EMPTY;

    if (cwd != NULL)
    {
        path_copy(&ctx->cwd, cwd);
    }

    for (uint64_t i = 0; i < CONFIG_MAX_FD; i++)
    {
        ctx->files[i] = NULL;
    }
    lock_init(&ctx->lock);
    ctx->initalized = true;
}

void vfs_ctx_deinit(vfs_ctx_t* ctx)
{
    LOCK_SCOPE(&ctx->lock);

    path_put(&ctx->cwd);

    for (uint64_t i = 0; i < CONFIG_MAX_FD; i++)
    {
        if (ctx->files[i] != NULL)
        {
            DEREF(ctx->files[i]);
            ctx->files[i] = NULL;
        }
    }

    ctx->initalized = false;
}

file_t* vfs_ctx_get_file(vfs_ctx_t* ctx, fd_t fd)
{
    LOCK_SCOPE(&ctx->lock);

    if (!ctx->initalized)
    {
        errno = EBUSY;
        return NULL;
    }

    if (fd >= CONFIG_MAX_FD || ctx->files[fd] == NULL)
    {
        errno = EBADF;
        return NULL;
    }

    return REF(ctx->files[fd]);
}

uint64_t vfs_ctx_get_cwd(vfs_ctx_t* ctx, path_t* outCwd)
{
    LOCK_SCOPE(&ctx->lock);

    if (!ctx->initalized)
    {
        errno = EBUSY;
        return ERR;
    }

    if (ctx->cwd.dentry == NULL || ctx->cwd.mount == NULL)
    {
        assert(ctx->cwd.dentry == NULL && ctx->cwd.mount == NULL);
        namespace_t* kernelNs = &process_get_kernel()->namespace;

        if (namespace_get_root_path(kernelNs, outCwd) == ERR)
        {
            return ERR;
        }
        return 0;
    }

    path_copy(outCwd, &ctx->cwd);
    return 0;
}

uint64_t vfs_ctx_set_cwd(vfs_ctx_t* ctx, const path_t* cwd)
{
    LOCK_SCOPE(&ctx->lock);

    if (!ctx->initalized)
    {
        errno = EBUSY;
        return ERR;
    }

    if (cwd == NULL || cwd->dentry == NULL || cwd->dentry->inode == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (cwd->dentry->inode->type != INODE_DIR)
    {
        errno = ENOTDIR;
        return ERR;
    }

    path_put(&ctx->cwd);
    path_copy(&ctx->cwd, cwd);

    return 0;
}

SYSCALL_DEFINE(SYS_CHDIR, uint64_t, const char* pathString)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    pathname_t pathname;
    if (space_safe_pathname_init(space, &pathname, pathString) == ERR)
    {
        return ERR;
    }

    path_t path = PATH_EMPTY;
    if (vfs_walk(&path, &pathname, WALK_NONE, process) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&path);

    return vfs_ctx_set_cwd(&process->vfsCtx, &path);
}

fd_t vfs_ctx_open(vfs_ctx_t* ctx, file_t* file)
{
    LOCK_SCOPE(&ctx->lock);

    if (!ctx->initalized)
    {
        errno = EBUSY;
        return ERR;
    }

    for (fd_t fd = 0; fd < CONFIG_MAX_FD; fd++)
    {
        if (ctx->files[fd] == NULL)
        {
            ctx->files[fd] = REF(file);
            return fd;
        }
    }

    errno = EMFILE;
    return ERR;
}

fd_t vfs_ctx_openas(vfs_ctx_t* ctx, fd_t fd, file_t* file)
{
    LOCK_SCOPE(&ctx->lock);

    if (!ctx->initalized)
    {
        errno = EBUSY;
        return ERR;
    }

    if (fd >= CONFIG_MAX_FD)
    {
        errno = EINVAL;
        return ERR;
    }

    if (ctx->files[fd] != NULL)
    {
        DEREF(ctx->files[fd]);
        ctx->files[fd] = NULL;
    }

    ctx->files[fd] = REF(file);
    return fd;
}

uint64_t vfs_ctx_close(vfs_ctx_t* ctx, fd_t fd)
{
    LOCK_SCOPE(&ctx->lock);

    if (!ctx->initalized)
    {
        errno = EBUSY;
        return ERR;
    }

    if (fd >= CONFIG_MAX_FD || ctx->files[fd] == NULL)
    {
        errno = EBADF;
        return ERR;
    }

    DEREF(ctx->files[fd]);
    ctx->files[fd] = NULL;
    return 0;
}

SYSCALL_DEFINE(SYS_CLOSE, uint64_t, fd_t fd)
{
    return vfs_ctx_close(&sched_process()->vfsCtx, fd);
}

fd_t vfs_ctx_dup(vfs_ctx_t* ctx, fd_t oldFd)
{
    LOCK_SCOPE(&ctx->lock);

    if (!ctx->initalized)
    {
        errno = EBUSY;
        return ERR;
    }

    if (oldFd >= CONFIG_MAX_FD || ctx->files[oldFd] == NULL)
    {
        errno = EBADF;
        return ERR;
    }

    for (fd_t fd = 0; fd < CONFIG_MAX_FD; fd++)
    {
        if (ctx->files[fd] == NULL)
        {
            ctx->files[fd] = REF(ctx->files[oldFd]);
            return fd;
        }
    }

    errno = EMFILE;
    return ERR;
}

SYSCALL_DEFINE(SYS_DUP, uint64_t, fd_t oldFd)
{
    return vfs_ctx_dup(&sched_process()->vfsCtx, oldFd);
}

fd_t vfs_ctx_dup2(vfs_ctx_t* ctx, fd_t oldFd, fd_t newFd)
{
    if (oldFd == newFd)
    {
        return newFd;
    }

    LOCK_SCOPE(&ctx->lock);

    if (!ctx->initalized)
    {
        errno = EBUSY;
        return ERR;
    }

    if (oldFd >= CONFIG_MAX_FD || newFd >= CONFIG_MAX_FD || ctx->files[oldFd] == NULL)
    {
        errno = EBADF;
        return ERR;
    }

    if (ctx->files[newFd] != NULL)
    {
        DEREF(ctx->files[newFd]);
        ctx->files[newFd] = NULL;
    }

    ctx->files[newFd] = REF(ctx->files[oldFd]);

    return newFd;
}

SYSCALL_DEFINE(SYS_DUP2, uint64_t, fd_t oldFd, fd_t newFd)
{
    return vfs_ctx_dup2(&sched_process()->vfsCtx, oldFd, newFd);
}
