#include <_libstd/MAX_PATH.h>
#include <kernel/fs/vfs.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/cwd.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/key.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/io.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/rcu.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/ref.h>

#include <kernel/cpu/regs.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/list.h>

static status_t vfs_create(path_t* path, const pathname_t* pathname, namespace_t* ns)
{
    path_t parent = PATH_EMPTY;
    path_t target = PATH_EMPTY;
    status_t status = path_walk_parent_and_child(path, &parent, &target, pathname, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    PATH_DEFER(&parent);
    PATH_DEFER(&target);

    if (!DENTRY_IS_POSITIVE(parent.dentry))
    {
        return ERR(VFS, NOENT);
    }

    vnode_t* dir = parent.dentry->vnode;
    if (dir->cls == NULL || dir->cls->create == NULL)
    {
        return ERR(VFS, PERM);
    }

    MUTEX_SCOPE(&dir->mutex);

    if (DENTRY_IS_POSITIVE(target.dentry))
    {
        if (pathname->mode & MODE_EXCLUSIVE)
        {
            return ERR(VFS, EXIST);
        }

        path_copy(path, &target);
        return OK;
    }

    if (!(parent.mount->mode & MODE_WRITE))
    {
        return ERR(VFS, ACCESS);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    status = dir->cls->create(dir, target.dentry, pathname->mode);
    if (IS_ERR(status))
    {
        return status;
    }

    path_copy(path, &target);
    return OK;
}

static status_t vfs_open_lookup(path_t* path, const pathname_t* pathname, namespace_t* namespace)
{
    if (pathname->mode & MODE_CREATE)
    {
        return vfs_create(path, pathname, namespace);
    }

    return path_walk(path, pathname, namespace);
}

status_t vfs_open(file_t** out, const pathname_t* pathname, process_t* process)
{
    if (out == NULL || pathname == NULL || process == NULL)
    {
        return ERR(VFS, INVAL);
    }

    return vfs_openat(out, NULL, pathname, process);
}

status_t vfs_openat(file_t** out, const path_t* from, const pathname_t* pathname, process_t* process)
{
    if (out == NULL || pathname == NULL || process == NULL)
    {
        return ERR(VFS, INVAL);
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(VFS, DYING);
    }
    UNREF_DEFER(ns);

    path_t path;
    if (from != NULL)
    {
        path = PATH_CREATE(from->mount, from->dentry);
    }
    else
    {
        path = cwd_get(&process->cwd, ns);
    }
    PATH_DEFER(&path);

    status_t status = vfs_open_lookup(&path, pathname, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    mode_t mode = pathname->mode;
    status = mode_check(&mode, path.mount->mode);
    if (IS_ERR(status))
    {
        return status;
    }

    if (!DENTRY_IS_POSITIVE(path.dentry))
    {
        return ERR(VFS, NOENT);
    }

    file_t* file = file_new(&path, mode);
    if (file == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    if (pathname->mode & MODE_TRUNCATE && file->vnode->cls->type == VNODE_REGULAR)
    {
        vnode_truncate(file->vnode);
    }

    if (file->vnode->cls->file_ctor != NULL)
    {
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        status = file->vnode->cls->file_ctor(file);
        if (IS_ERR(status))
        {
            UNREF(file);
            return status;
        }
    }

    *out = file;
    return OK;
}

typedef struct
{
    wait_queue_t wait;
    status_t status;
    uint64_t result;
    atomic_bool done;
} vfs_sync_ctx_t;

static status_t vfs_sync_complete(irp_t* irp, void* _ctx)
{
    vfs_sync_ctx_t* ctx = (vfs_sync_ctx_t*)_ctx;
    ctx->status = irp->status;
    ctx->result = irp->result;
    atomic_store(&ctx->done, true);
    wait_unblock(&ctx->wait, WAIT_ALL, OK);
    return OK;
}

static status_t vfs_run_sync(irp_t* irp, file_t* file, uint64_t* result)
{
    vfs_sync_ctx_t ctx;
    wait_queue_init(&ctx.wait);
    atomic_init(&ctx.done, false);

    irp_set_complete(irp, vfs_sync_complete, &ctx);
    file_call(file, irp);

    WAIT_BLOCK(&ctx.wait, atomic_load(&ctx.done));
    *result = ctx.result;
    return ctx.status;
}

status_t vfs_read(file_t* file, void* buffer, size_t count, size_t* out)
{
    if (file == NULL || buffer == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (!(file->mode & MODE_READ))
    {
        return ERR(VFS, BADFD);
    }

    irp_pool_t* pool = NULL;
    status_t status = irp_pool_new(&pool, 4, process_current(), NULL);
    if (IS_ERR(status))
    {
        return status;
    }

    irp_t* irp = NULL;
    status = irp_get(pool, &irp);
    if (IS_ERR(status))
    {
        irp_pool_free(pool);
        return status;
    }

    mdl_t* mdl;
    status = irp_get_mdl(irp, &mdl, buffer, count);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        irp_pool_free(pool);
        return status;
    }

    irp_prep_read(irp, mdl, IOOFF_CUR);
    uint64_t result = 0;
    status = vfs_run_sync(irp, file, &result);
    if (out != NULL)
    {
        *out = result;
    }
    return status;
}

status_t vfs_write(file_t* file, const void* buffer, size_t count, size_t* out)
{
    if (file == NULL || buffer == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (!(file->mode & MODE_WRITE))
    {
        return ERR(VFS, BADFD);
    }

    irp_pool_t* pool = NULL;
    status_t status = irp_pool_new(&pool, 4, process_current(), NULL);
    if (IS_ERR(status))
    {
        return status;
    }

    irp_t* irp = NULL;
    status = irp_get(pool, &irp);
    if (IS_ERR(status))
    {
        irp_pool_free(pool);
        return status;
    }

    mdl_t* mdl;
    status = irp_get_mdl(irp, &mdl, buffer, count);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        irp_pool_free(pool);
        return status;
    }

    irp_prep_write(irp, mdl, IOOFF_CUR);
    uint64_t result = 0;
    status = vfs_run_sync(irp, file, &result);
    if (out != NULL)
    {
        *out = result;
    }
    return status;
}

status_t vfs_seek(file_t* file, ssize_t offset, iowhence_t origin, size_t* out)
{
    if (file == NULL)
    {
        return ERR(VFS, INVAL);
    }

    irp_pool_t* pool = NULL;
    status_t status = irp_pool_new(&pool, 4, process_current(), NULL);
    if (IS_ERR(status))
    {
        return status;
    }

    irp_t* irp = NULL;
    status = irp_get(pool, &irp);
    if (IS_ERR(status))
    {
        irp_pool_free(pool);
        return status;
    }

    irp_prep_seek(irp, offset, origin);
    uint64_t result = 0;
    status = vfs_run_sync(irp, file, &result);
    if (out != NULL)
    {
        *out = result;
    }
    return status;
}

typedef struct
{
    wait_queue_t wait;
    atomic_size_t triggered;
    atomic_size_t completed;
} vfs_poll_ctx_t;

static void vfs_poll_complete(irp_t* irp, void* _ctx)
{
    vfs_poll_ctx_t* ctx = _ctx;
    if (IS_OK(irp->status))
    {
        atomic_fetch_add(&ctx->triggered, 1);
    }
    atomic_fetch_add(&ctx->completed, 1);
    wait_unblock(&ctx->wait, WAIT_ALL, OK);
}

typedef struct
{
    dirent_t* buffer;
    uint64_t count;
    uint64_t pos;
    uint64_t skip;
    uint64_t currentOffset;
} getdents_recursive_ctx_t;

typedef struct
{
    dir_ctx_t ctx;
    dirent_t* buffer;
    uint64_t count;
    uint64_t written;
    path_t path;
    namespace_t* ns;
    bool more;
} vfs_dir_ctx_t;

static bool vfs_dir_emit(dir_ctx_t* ctx, const char* name, vnode_type_t type)
{
    vfs_dir_ctx_t* vctx = (vfs_dir_ctx_t*)ctx;
    if (vctx->written + sizeof(dirent_t) > vctx->count)
    {
        vctx->more = true;
        return false;
    }

    rcu_read_lock();

    dirent_flags_t flags = DIRENT_NONE;
    mode_t mode = vctx->path.mount->mode;
    dentry_t* child = dentry_rcu_get(vctx->path.dentry, name, strlen(name));
    if (REF_COUNT(child) != 0)
    {
        mount_t* mount = vctx->path.mount;
        dentry_t* dentry = child;
        if (namespace_rcu_traverse(vctx->ns, &mount, &dentry))
        {
            type = dentry->vnode->cls->type;
            mode = mount->mode;
            flags |= DIRENT_MOUNTED;
        }
    }

    rcu_read_unlock();

    dirent_t* d = (dirent_t*)((uint8_t*)vctx->buffer + vctx->written);
    d->type = type;
    d->flags = flags;
    strncpy(d->path, name, MAX_PATH - 1);
    d->path[MAX_PATH - 1] = '\0';
    mode_to_string(mode, d->mode, MAX_PATH, NULL);

    vctx->written += sizeof(dirent_t);
    vctx->ctx.pos++;
    return true;
}

static status_t vfs_getdents_recursive_step(path_t* path, mode_t mode, getdents_recursive_ctx_t* ctx,
    const char* prefix, namespace_t* ns)
{
    uint64_t offset = 0;
    uint64_t bufSize = 1024;
    dirent_t* buf = malloc(bufSize);
    if (buf == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    while (true)
    {
        vfs_dir_ctx_t vctx = {
            .ctx = {.emit = vfs_dir_emit, .pos = offset},
            .buffer = buf,
            .count = bufSize,
            .written = 0,
            .path = *path,
            .ns = ns,
            .more = false,
        };

        path->dentry->vnode->cls->iterate(path->dentry, &vctx.ctx);
        offset = vctx.ctx.pos;

        if (vctx.written == 0)
        {
            break;
        }

        uint64_t pos = 0;
        while (pos < vctx.written)
        {
            dirent_t* d = (dirent_t*)((uint8_t*)buf + pos);
            pos += sizeof(dirent_t);

            if (ctx->currentOffset >= ctx->skip)
            {
                if (ctx->pos + sizeof(dirent_t) > ctx->count)
                {
                    free(buf);
                    return INFO(FS, MORE);
                }

                dirent_t* out = (dirent_t*)((uint8_t*)ctx->buffer + ctx->pos);
                *out = *d;

                if (prefix[0] != '\0')
                {
                    if (strcmp(d->path, ".") != 0 && strcmp(d->path, "..") != 0)
                    {
                        char tmp[MAX_PATH];
                        strncpy(tmp, d->path, MAX_PATH);
                        snprintf(out->path, MAX_PATH, "%s/%s", prefix, tmp);
                    }
                    else
                    {
                        snprintf(out->path, MAX_PATH, "%s/%s", prefix, d->path);
                    }
                }

                ctx->pos += sizeof(dirent_t);
            }
            ctx->currentOffset += sizeof(dirent_t);

            if ((d->type == VNODE_DIR || d->type == VNODE_SYMLINK) && strcmp(d->path, ".") != 0 &&
                strcmp(d->path, "..") != 0)
            {
                path_t childPath = PATH_CREATE(path->mount, path->dentry);
                PATH_DEFER(&childPath);

                status_t status = path_step(&childPath, mode, d->path, ns);
                if (IS_ERR(status))
                {
                    free(buf);
                    return status;
                }

                if (!DENTRY_IS_DIR(childPath.dentry))
                {
                    continue;
                }

                char newPrefix[MAX_PATH];
                if (prefix[0] == '\0')
                {
                    snprintf(newPrefix, MAX_PATH, "%s", d->path);
                }
                else
                {
                    snprintf(newPrefix, MAX_PATH, "%s/%s", prefix, d->path);
                }

                status = vfs_getdents_recursive_step(&childPath, mode, ctx, newPrefix, ns);
                if (IS_ERR(status) || status == INFO(FS, MORE))
                {
                    free(buf);
                    return status;
                }
                path_put(&childPath);
            }
        }
    }

    free(buf);
    return OK;
}

static status_t vfs_remove_recursive(path_t* path, process_t* process)
{
    if (DENTRY_IS_ROOT(path->dentry))
    {
        return ERR(VFS, BUSY);
    }

    if (!DENTRY_IS_DIR(path->dentry))
    {
        vnode_t* dir = path->dentry->parent->vnode;
        if (IS_ERR(dir->cls->remove(dir, path->dentry)))
        {
            return ERR(VFS, IO);
        }
        return OK;
    }

    getdents_recursive_ctx_t ctx = {.buffer = NULL, .count = 0, .pos = 0, .skip = 0, .currentOffset = 0};

    uint64_t offset = 0;
    uint64_t bufSize = 1024;
    dirent_t* buf = malloc(bufSize);
    if (buf == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    while (true)
    {
        vfs_dir_ctx_t vctx = {
            .ctx = {.emit = vfs_dir_emit, .pos = offset},
            .buffer = buf,
            .count = bufSize,
            .written = 0,
            .path = *path,
            .ns = process_get_ns(process),
        };
        if (vctx.ns == NULL)
        {
            return ERR(VFS, DYING);
        }

        UNREF_DEFER(vctx.ns);

        path->dentry->vnode->cls->iterate(path->dentry, &vctx.ctx);
        offset = vctx.ctx.pos;

        if (vctx.written == 0)
        {
            break;
        }

        uint64_t pos = 0;
        bool removed = false;
        while (pos < vctx.written)
        {
            dirent_t* d = (dirent_t*)((uint8_t*)buf + pos);
            pos += sizeof(dirent_t);

            if (strcmp(d->path, ".") == 0 || strcmp(d->path, "..") == 0)
            {
                continue;
            }

            path_t childPath = PATH_CREATE(path->mount, path->dentry);
            PATH_DEFER(&childPath);

            status_t status = path_step(&childPath, MODE_NONE, d->path, vctx.ns);
            if (IS_ERR(status))
            {
                free(buf);
                return status;
            }

            status = vfs_remove_recursive(&childPath, process);
            if (IS_ERR(status))
            {
                free(buf);
                return status;
            }

            removed = true;
            break;
        }

        if (removed)
        {
            offset = 0;
            continue;
        }
    }

    free(buf);

    vnode_t* dir = path->dentry->parent->vnode;
    if (dir->cls->remove == NULL)
    {
        return ERR(VFS, PERM);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    return dir->cls->remove(dir, path->dentry);
}

status_t vfs_getdents(file_t* file, dirent_t* buffer, size_t count, size_t* bytesRead)
{
    if (file == NULL || (buffer == NULL && count > 0) || bytesRead == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (file->vnode == NULL || file->vnode->cls->type != VNODE_DIR)
    {
        return ERR(VFS, NOTDIR);
    }

    if (file->path.dentry == NULL || file->path.dentry->parent == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (file->path.dentry->vnode->cls->iterate == NULL)
    {
        return ERR(VFS, IMPL);
    }

    if (!(file->mode & MODE_READ))
    {
        return ERR(VFS, BADFD);
    }

    process_t* process = process_current();
    assert(process != NULL);

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(VFS, DYING);
    }
    UNREF_DEFER(ns);

    MUTEX_SCOPE(&file->vnode->mutex);

    if (file->mode & MODE_RECURSIVE)
    {
        getdents_recursive_ctx_t ctx = {
            .buffer = buffer,
            .count = count,
            .pos = 0,
            .skip = file->pos,
            .currentOffset = 0,
        };
        status_t status = vfs_getdents_recursive_step(&file->path, file->mode, &ctx, "", ns);
        if (IS_ERR(status))
        {
            return status;
        }
        file->pos = ctx.skip + ctx.pos;
        *bytesRead = ctx.pos;
        return status;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    vfs_dir_ctx_t ctx = {.ctx = {.emit = vfs_dir_emit, .pos = file->pos},
        .buffer = buffer,
        .count = count,
        .written = 0,
        .path = file->path,
        .ns = ns,
        .more = false};

    status_t status = file->path.dentry->vnode->cls->iterate(file->path.dentry, &ctx.ctx);
    file->pos = ctx.ctx.pos;

    if (IS_OK(status))
    {
        *bytesRead = ctx.written;
        if (ctx.more)
        {
            return INFO(FS, MORE);
        }
    }
    return status;
}

status_t vfs_stat(const pathname_t* pathname, stat_t* buffer, process_t* process)
{
    if (pathname == NULL || buffer == NULL || process == NULL)
    {
        return ERR(VFS, INVAL);
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(VFS, DYING);
    }
    UNREF_DEFER(ns);

    path_t path = cwd_get(&process->cwd, ns);
    PATH_DEFER(&path);

    status_t status = path_walk(&path, pathname, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    if (!(path.mount->mode & MODE_READ))
    {
        return ERR(VFS, ACCESS);
    }

    memset(buffer, 0, sizeof(stat_t));

    if (!DENTRY_IS_POSITIVE(path.dentry))
    {
        return ERR(VFS, NOENT);
    }

    /// @todo Reimplement this after the async system.
    vnode_t* vnode = path.dentry->vnode;
    mutex_acquire(&vnode->mutex);
    buffer->number = 0;
    buffer->type = vnode->cls->type;
    buffer->size = vnode->size;
    buffer->blocks = 0;
    buffer->linkAmount = atomic_load(&vnode->dentryCount);
    buffer->accessTime = 0;
    buffer->modifyTime = 0;
    buffer->changeTime = 0;
    buffer->createTime = 0;

    char mode[MAX_PATH];
    status = mode_to_string(path.mount->mode, mode, MAX_PATH, NULL);
    if (IS_ERR(status))
    {
        mutex_release(&vnode->mutex);
        return status;
    }

    if (snprintf(buffer->name, sizeof(buffer->name), "%s%s", path.dentry->name, mode) < 0)
    {
        mutex_release(&vnode->mutex);
        return ERR(VFS, IO);
    }

    mutex_release(&vnode->mutex);
    return OK;
}

status_t vfs_link(const pathname_t* oldPathname, const pathname_t* newPathname, process_t* process)
{
    if (oldPathname == NULL || newPathname == NULL || process == NULL)
    {
        return ERR(VFS, INVAL);
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(VFS, DYING);
    }
    UNREF_DEFER(ns);

    path_t cwd = cwd_get(&process->cwd, ns);
    PATH_DEFER(&cwd);

    path_t oldParent = PATH_EMPTY;
    path_t old = PATH_EMPTY;
    PATH_DEFER(&oldParent);
    PATH_DEFER(&old);

    status_t status = path_walk_parent_and_child(&cwd, &oldParent, &old, oldPathname, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    path_t newParent = PATH_EMPTY;
    path_t new = PATH_EMPTY;
    PATH_DEFER(&newParent);
    PATH_DEFER(&new);

    status = path_walk_parent_and_child(&cwd, &newParent, &new, newPathname, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    if (oldParent.dentry->volume != newParent.dentry->volume)
    {
        return ERR(VFS, XDEV);
    }

    if (!DENTRY_IS_POSITIVE(old.dentry))
    {
        return ERR(VFS, NOENT);
    }

    if (DENTRY_IS_DIR(old.dentry))
    {
        return ERR(VFS, ISDIR);
    }

    if (!DENTRY_IS_POSITIVE(newParent.dentry))
    {
        return ERR(VFS, NOENT);
    }

    if (newParent.dentry->vnode->cls->link == NULL)
    {
        return ERR(VFS, PERM);
    }

    if (!(old.mount->mode & MODE_READ))
    {
        return ERR(VFS, ACCESS);
    }

    if (!(newParent.mount->mode & MODE_WRITE))
    {
        return ERR(VFS, ACCESS);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    return newParent.dentry->vnode->cls->link(newParent.dentry->vnode, old.dentry, new.dentry);
}

status_t vfs_readlink(vnode_t* symlink, char* buffer, size_t count, size_t* bytesRead)
{
    if (symlink == NULL || buffer == NULL || count == 0 || bytesRead == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (symlink->cls->readlink == NULL)
    {
        return ERR(VFS, INVAL);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    return symlink->cls->readlink(symlink, buffer, count, bytesRead);
}

status_t vfs_symlink(const pathname_t* oldPathname, const pathname_t* newPathname, process_t* process)
{
    if (oldPathname == NULL || newPathname == NULL || process == NULL)
    {
        return ERR(VFS, INVAL);
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(VFS, DYING);
    }
    UNREF_DEFER(ns);

    path_t cwd = cwd_get(&process->cwd, ns);
    PATH_DEFER(&cwd);

    path_t newParent = PATH_EMPTY;
    path_t new = PATH_EMPTY;
    PATH_DEFER(&newParent);
    PATH_DEFER(&new);

    status_t status = path_walk_parent_and_child(&cwd, &newParent, &new, newPathname, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    if (!DENTRY_IS_POSITIVE(newParent.dentry))
    {
        return ERR(VFS, NOENT);
    }

    if (DENTRY_IS_POSITIVE(new.dentry))
    {
        return ERR(VFS, EXIST);
    }

    if (newParent.dentry->vnode->cls->symlink == NULL)
    {
        return ERR(VFS, PERM);
    }

    if (!(newParent.mount->mode & MODE_WRITE))
    {
        return ERR(VFS, ACCESS);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    return newParent.dentry->vnode->cls->symlink(newParent.dentry->vnode, new.dentry, oldPathname->string);
}

status_t vfs_remove(const pathname_t* pathname, process_t* process)
{
    if (pathname == NULL || process == NULL)
    {
        return ERR(VFS, INVAL);
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(VFS, DYING);
    }
    UNREF_DEFER(ns);

    path_t cwd = cwd_get(&process->cwd, ns);
    PATH_DEFER(&cwd);

    path_t parent = PATH_EMPTY;
    path_t target = PATH_EMPTY;
    status_t status = path_walk_parent_and_child(&cwd, &parent, &target, pathname, ns);
    if (IS_ERR(status))
    {
        return status;
    }
    PATH_DEFER(&parent);
    PATH_DEFER(&target);

    if (!DENTRY_IS_POSITIVE(target.dentry))
    {
        return ERR(VFS, NOENT);
    }

    if (!(pathname->mode & MODE_RECURSIVE))
    {
        if (pathname->mode & MODE_DIRECTORY)
        {
            if (!DENTRY_IS_DIR(target.dentry))
            {
                return ERR(VFS, NOTDIR);
            }
        }
        else
        {
            if (DENTRY_IS_DIR(target.dentry))
            {
                return ERR(VFS, ISDIR);
            }
        }
    }

    if (!(target.mount->mode & MODE_WRITE))
    {
        return ERR(VFS, ACCESS);
    }

    if (pathname->mode & MODE_RECURSIVE)
    {
        return vfs_remove_recursive(&target, process);
    }

    vnode_t* dir = parent.dentry->vnode;
    if (dir->cls->remove == NULL)
    {
        return ERR(VFS, PERM);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    return dir->cls->remove(dir, target.dentry);
}

uint64_t vfs_id_get(void)
{
    static _Atomic(uint64_t) newVfsId = ATOMIC_VAR_INIT(0);

    return atomic_fetch_add(&newVfsId, 1);
}

SYSCALL_DEFINE(SYS_OPEN, const char* pathString)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    pathname_t pathname;
    status_t status = thread_copy_from_user_pathname(thread, &pathname, pathString);
    if (IS_ERR(status))
    {
        return status;
    }

    file_t* file = NULL;
    status = vfs_open(&file, &pathname, process);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(file);

    *_result = file_table_open(&process->files, file);
    return OK;
}

SYSCALL_DEFINE(SYS_OPENAT, fd_t from, const char* pathString)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    path_t fromPath = PATH_EMPTY;
    if (from != FD_NONE)
    {
        file_t* fromFile = file_table_get(&process->files, from);
        if (fromFile == NULL)
        {
            return ERR(VFS, BADFD);
        }
        path_copy(&fromPath, &fromFile->path);
        UNREF(fromFile);
    }
    PATH_DEFER(&fromPath);

    pathname_t pathname;
    status_t status = thread_copy_from_user_pathname(thread, &pathname, pathString);
    if (IS_ERR(status))
    {
        return status;
    }

    file_t* file = NULL;
    status = vfs_openat(&file, from != FD_NONE ? &fromPath : NULL, &pathname, process);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(file);

    *_result = file_table_open(&process->files, file);
    return OK;
}

SYSCALL_DEFINE(SYS_GETDENTS, fd_t fd, dirent_t* buffer, uint64_t count)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    file_t* file = file_table_get(&process->files, fd);
    if (file == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(file);

    status_t status = space_pin(&process->space, buffer, count, &thread->userStack);
    if (IS_ERR(status))
    {
        return status;
    }
    size_t bytesWritten = 0;
    status = vfs_getdents(file, buffer, count, &bytesWritten);
    space_unpin(&process->space, buffer, count);
    if (IS_OK(status))
    {
        *_result = bytesWritten;
    }
    return status;
}

SYSCALL_DEFINE(SYS_STAT, const char* pathString, stat_t* buffer)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    pathname_t pathname;
    status_t status = thread_copy_from_user_pathname(thread, &pathname, pathString);
    if (IS_ERR(status))
    {
        return status;
    }

    status = space_pin(&process->space, buffer, sizeof(stat_t), &thread->userStack);
    if (IS_ERR(status))
    {
        return status;
    }
    status = vfs_stat(&pathname, buffer, process);
    space_unpin(&process->space, buffer, sizeof(stat_t));
    return status;
}

SYSCALL_DEFINE(SYS_LINK, const char* oldPathString, const char* newPathString)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    pathname_t oldPathname;
    status_t status = thread_copy_from_user_pathname(thread, &oldPathname, oldPathString);
    if (IS_ERR(status))
    {
        return status;
    }

    pathname_t newPathname;
    status = thread_copy_from_user_pathname(thread, &newPathname, newPathString);
    if (IS_ERR(status))
    {
        return status;
    }

    return vfs_link(&oldPathname, &newPathname, process);
}

SYSCALL_DEFINE(SYS_READLINK, const char* pathString, char* buffer, uint64_t count)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    pathname_t pathname;
    status_t status = thread_copy_from_user_pathname(thread, &pathname, pathString);
    if (IS_ERR(status))
    {
        return status;
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(VFS, DYING);
    }
    UNREF_DEFER(ns);

    path_t path = cwd_get(&process->cwd, ns);
    PATH_DEFER(&path);

    status = path_walk(&path, &pathname, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    if (!DENTRY_IS_POSITIVE(path.dentry))
    {
        return ERR(VFS, NOENT);
    }

    status = space_pin(&process->space, buffer, count, &thread->userStack);
    if (IS_ERR(status))
    {
        return status;
    }
    size_t bytesRead = 0;
    status = vfs_readlink(path.dentry->vnode, buffer, count, &bytesRead);
    space_unpin(&process->space, buffer, count);
    if (IS_OK(status))
    {
        *_result = bytesRead;
    }
    return status;
}

SYSCALL_DEFINE(SYS_SYMLINK, const char* targetString, const char* linkpathString)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    pathname_t target;
    status_t status = thread_copy_from_user_pathname(thread, &target, targetString);
    if (IS_ERR(status))
    {
        return status;
    }

    pathname_t linkpath;
    status = thread_copy_from_user_pathname(thread, &linkpath, linkpathString);
    if (IS_ERR(status))
    {
        return status;
    }

    return vfs_symlink(&target, &linkpath, process);
}

SYSCALL_DEFINE(SYS_REMOVE, const char* pathString)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    pathname_t pathname;
    status_t status = thread_copy_from_user_pathname(thread, &pathname, pathString);
    if (IS_ERR(status))
    {
        return status;
    }

    return vfs_remove(&pathname, process);
}
