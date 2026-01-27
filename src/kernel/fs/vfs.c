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
        if (!IS_CODE(status, NOENT) || !(pathname->mode & MODE_PARENTS))
        {
            return status;
        }

        char parentString[MAX_PATH];
        strncpy(parentString, pathname->string, MAX_PATH);
        parentString[MAX_PATH - 1] = '\0';

        size_t len = strlen(parentString);
        while (len > 1 && parentString[len - 1] == '/')
        {
            parentString[--len] = '\0';
        }

        char* lastSlash = strrchr(parentString, '/');
        if (lastSlash == NULL)
        {
            return ERR(VFS, INVAL);
        }

        if (lastSlash == parentString)
        {
            parentString[1] = '\0';
        }
        else
        {
            *lastSlash = '\0';
        }

        pathname_t parentPathname;
        status_t status = pathname_init(&parentPathname, parentString);
        if (IS_ERR(status))
        {
            return ERR(VFS, INVAL);
        }

        parentPathname.mode = MODE_DIRECTORY | MODE_CREATE | MODE_PARENTS;

        path_t recursiveStart = PATH_CREATE(path->mount, path->dentry);
        PATH_DEFER(&recursiveStart);

        status = vfs_create(&recursiveStart, &parentPathname, ns);
        if (IS_ERR(status))
        {
            return status;
        }

        status = path_walk_parent_and_child(path, &parent, &target, pathname, ns);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    PATH_DEFER(&parent);
    PATH_DEFER(&target);

    if (!DENTRY_IS_POSITIVE(parent.dentry))
    {
        return ERR(VFS, NOENT);
    }

    vnode_t* dir = parent.dentry->vnode;
    if (dir->ops == NULL || dir->ops->create == NULL)
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
    status = dir->ops->create(dir, target.dentry, pathname->mode);
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

status_t vfs_open2(const pathname_t* pathname, file_t* files[2], process_t* process)
{
    if (pathname == NULL || files == NULL || process == NULL)
    {
        return ERR(VFS, INVAL);
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(VFS, INVAL);
    }
    UNREF_DEFER(ns);

    path_t path = cwd_get(&process->cwd, ns);
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

    files[0] = file_new(&path, mode);
    if (files[0] == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    files[1] = file_new(&path, mode);
    if (files[1] == NULL)
    {
        UNREF(files[0]);
        return ERR(VFS, NOMEM);
    }

    if (pathname->mode & MODE_TRUNCATE && files[0]->vnode->type == VREG)
    {
        vnode_truncate(files[0]->vnode);
    }

    if (files[0]->ops != NULL && files[0]->ops->open2 != NULL)
    {
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        status = files[0]->ops->open2(files);
        if (IS_ERR(status))
        {
            UNREF(files[0]);
            UNREF(files[1]);
            return status;
        }
    }

    return OK;
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
        return ERR(VFS, INVAL);
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

    if (pathname->mode & MODE_TRUNCATE && file->vnode->type == VREG)
    {
        vnode_truncate(file->vnode);
    }

    if (file->ops != NULL && file->ops->open != NULL)
    {
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        status = file->ops->open(file);
        if (IS_ERR(status))
        {
            UNREF(file);
            return status;
        }
    }

    *out = file;
    return OK;
}

status_t vfs_read(file_t* file, void* buffer, size_t count, size_t* bytesRead)
{
    if (file == NULL || buffer == NULL || bytesRead == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (file->vnode->type == VDIR)
    {
        return ERR(VFS, ISDIR);
    }

    if (file->ops == NULL || file->ops->read == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (!(file->mode & MODE_READ))
    {
        return ERR(VFS, BADFD);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    size_t offset = file->pos;
    status_t status = file->ops->read(file, buffer, count, &offset, bytesRead);
    if (IS_OK(status))
    {
        file->pos = offset;
    }

    return status;
}

status_t vfs_write(file_t* file, const void* buffer, size_t count, size_t* bytesWritten)
{
    if (file == NULL || buffer == NULL || bytesWritten == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (file->vnode->type == VDIR)
    {
        return ERR(VFS, ISDIR);
    }

    if (file->ops == NULL || file->ops->write == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (file->mode & MODE_APPEND)
    {
        size_t newPos;
        if (file->ops->seek != NULL && IS_ERR(file->ops->seek(file, 0, SEEK_END, &newPos)))
        {
            return ERR(VFS, IO);
        }
    }

    if (!(file->mode & MODE_WRITE))
    {
        return ERR(VFS, BADFD);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    size_t offset = file->pos;
    status_t status = file->ops->write(file, buffer, count, &offset, bytesWritten);
    if (IS_OK(status))
    {
        file->pos = offset;
    }

    return status;
}

status_t vfs_seek(file_t* file, ssize_t offset, seek_origin_t origin, size_t* newPos)
{
    if (file == NULL || newPos == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (file->ops != NULL && file->ops->seek != NULL)
    {
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        return file->ops->seek(file, offset, origin, newPos);
    }

    return ERR(VFS, SPIPE);
}

status_t vfs_ioctl(file_t* file, uint64_t request, void* argp, size_t size, uint64_t* result)
{
    if (file == NULL || result == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (file->vnode->type == VDIR)
    {
        return ERR(VFS, ISDIR);
    }

    if (file->ops == NULL || file->ops->ioctl == NULL)
    {
        return ERR(VFS, NOTTY);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    return file->ops->ioctl(file, request, argp, size, result);
}

status_t vfs_mmap(file_t* file, void** addr, size_t length, pml_flags_t flags)
{
    if (file == NULL || addr == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (file->vnode->type == VDIR)
    {
        return ERR(VFS, ISDIR);
    }

    if (file->ops == NULL || file->ops->mmap == NULL)
    {
        return ERR(VFS, NODEV);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    size_t offset = file->pos;
    status_t status = file->ops->mmap(file, addr, length, &offset, flags);
    if (IS_OK(status))
    {
        file->pos = offset;
    }
    return status;
}

typedef struct
{
    wait_queue_t* queues[CONFIG_MAX_FD];
    uint16_t lookupTable[CONFIG_MAX_FD];
    uint16_t queueAmount;
} vfs_poll_ctx_t;

static status_t vfs_poll_ctx_init(vfs_poll_ctx_t* ctx, poll_file_t* files, uint64_t amount)
{
    memset(ctx->queues, 0, sizeof(wait_queue_t*) * CONFIG_MAX_FD);
    memset(ctx->lookupTable, 0, sizeof(uint16_t) * CONFIG_MAX_FD);
    ctx->queueAmount = 0;

    for (uint64_t i = 0; i < amount; i++)
    {
        files[i].revents = POLLNONE;
        wait_queue_t* queue = NULL;
        if (IS_ERR(files[i].file->ops->poll(files[i].file, &files[i].revents, &queue)))
        {
            return ERR(VFS, IO);
        }

        // Avoid duplicate queues.
        bool found = false;
        for (uint16_t j = 0; j < ctx->queueAmount; j++)
        {
            if (ctx->queues[j] == queue)
            {
                found = true;
                ctx->lookupTable[i] = j;
                break;
            }
        }

        if (!found)
        {
            ctx->queues[ctx->queueAmount] = queue;
            ctx->lookupTable[i] = ctx->queueAmount;
            ctx->queueAmount++;
        }
    }

    return OK;
}

static status_t vfs_poll_ctx_check_events(vfs_poll_ctx_t* ctx, poll_file_t* files, uint64_t amount, size_t* readyCount)
{
    *readyCount = 0;

    for (uint64_t i = 0; i < amount; i++)
    {
        poll_events_t revents = POLLNONE;
        wait_queue_t* queue = NULL;
        if (IS_ERR(files[i].file->ops->poll(files[i].file, &revents, &queue)))
        {
            return ERR(VFS, IO);
        }

        files[i].revents = (revents & (files[i].events | POLL_SPECIAL));

        if (queue != NULL && queue != ctx->queues[ctx->lookupTable[i]])
        {
            return ERR(VFS, IMPL);
        }

        if ((files[i].revents & (files[i].events | POLL_SPECIAL)) != 0)
        {
            (*readyCount)++;
        }
    }

    return OK;
}

status_t vfs_poll(poll_file_t* files, uint64_t amount, clock_t timeout, size_t* readyCount)
{
    if (files == NULL || amount == 0 || amount > CONFIG_MAX_FD || readyCount == NULL)
    {
        return ERR(VFS, INVAL);
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        if (files[i].file == NULL)
        {
            return ERR(VFS, INVAL);
        }

        if (files[i].file->vnode->type == VDIR)
        {
            return ERR(VFS, ISDIR);
        }

        if (files[i].file->ops == NULL || files[i].file->ops->poll == NULL)
        {
            return ERR(VFS, IMPL);
        }
    }

    vfs_poll_ctx_t ctx;
    status_t status = vfs_poll_ctx_init(&ctx, files, amount);
    if (IS_ERR(status))
    {
        return status;
    }

    clock_t uptime = clock_uptime();
    clock_t deadline = CLOCKS_DEADLINE(timeout, uptime);

    *readyCount = 0;
    while (true)
    {
        uptime = clock_uptime();
        clock_t remaining = CLOCKS_REMAINING(deadline, uptime);

        status = wait_block_prepare(ctx.queues, ctx.queueAmount, remaining);
        if (IS_ERR(status))
        {
            return status;
        }

        status = vfs_poll_ctx_check_events(&ctx, files, amount, readyCount);
        if (IS_ERR(status))
        {
            wait_block_cancel();
            return status;
        }

        if (*readyCount > 0 || uptime >= deadline)
        {
            wait_block_cancel();
            break;
        }

        status = wait_block_commit();
        if (IS_ERR(status))
        {
            if (IS_CODE(status, TIMEOUT))
            {
                break;
            }
            return status;
        }
    }

    return OK;
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

static bool vfs_dir_emit(dir_ctx_t* ctx, const char* name, vtype_t type)
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
            type = dentry->vnode->type;
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

        path->dentry->ops->iterate(path->dentry, &vctx.ctx);
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

            if ((d->type == VDIR || d->type == VSYMLINK) && strcmp(d->path, ".") != 0 && strcmp(d->path, "..") != 0)
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
        if (IS_ERR(dir->ops->remove(dir, path->dentry)))
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
        vfs_dir_ctx_t vctx = {.ctx = {.emit = vfs_dir_emit, .pos = offset},
            .buffer = buf,
            .count = bufSize,
            .written = 0,
            .path = *path,
            .ns = process_get_ns(process)};
        UNREF_DEFER(vctx.ns);

        path->dentry->ops->iterate(path->dentry, &vctx.ctx);
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
    if (dir->ops == NULL || dir->ops->remove == NULL)
    {
        return ERR(VFS, PERM);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    return dir->ops->remove(dir, path->dentry);
}

status_t vfs_getdents(file_t* file, dirent_t* buffer, size_t count, size_t* bytesRead)
{
    if (file == NULL || (buffer == NULL && count > 0) || bytesRead == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (file->vnode == NULL || file->vnode->type != VDIR)
    {
        return ERR(VFS, NOTDIR);
    }

    if (file->path.dentry == NULL || file->path.dentry->parent == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (file->path.dentry->ops == NULL || file->path.dentry->ops->iterate == NULL)
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
        return ERR(VFS, INVAL);
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

    status_t status = file->path.dentry->ops->iterate(file->path.dentry, &ctx.ctx);
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
        return ERR(VFS, INVAL);
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
    buffer->type = vnode->type;
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

    if (oldParent.dentry->superblock != newParent.dentry->superblock)
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

    if (newParent.dentry->vnode->ops == NULL || newParent.dentry->vnode->ops->link == NULL)
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
    return newParent.dentry->vnode->ops->link(newParent.dentry->vnode, old.dentry, new.dentry);
}

status_t vfs_readlink(vnode_t* symlink, char* buffer, size_t count, size_t* bytesRead)
{
    if (symlink == NULL || buffer == NULL || count == 0 || bytesRead == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (symlink->type != VSYMLINK)
    {
        return ERR(VFS, INVAL);
    }

    if (symlink->ops == NULL || symlink->ops->readlink == NULL)
    {
        return ERR(VFS, INVAL);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    return symlink->ops->readlink(symlink, buffer, count, bytesRead);
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
        return ERR(VFS, INVAL);
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

    if (newParent.dentry->vnode->ops == NULL || newParent.dentry->vnode->ops->symlink == NULL)
    {
        return ERR(VFS, PERM);
    }

    if (!(newParent.mount->mode & MODE_WRITE))
    {
        return ERR(VFS, ACCESS);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    return newParent.dentry->vnode->ops->symlink(newParent.dentry->vnode, new.dentry, oldPathname->string);
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
        return ERR(VFS, INVAL);
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
    if (dir->ops == NULL || dir->ops->remove == NULL)
    {
        return ERR(VFS, PERM);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    return dir->ops->remove(dir, target.dentry);
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

SYSCALL_DEFINE(SYS_OPEN2, const char* pathString, fd_t fds[2])
{
    if (fds == NULL)
    {
        return ERR(VFS, INVAL);
    }

    thread_t* thread = thread_current();
    process_t* process = thread->process;

    pathname_t pathname;
    status_t status = thread_copy_from_user_pathname(thread, &pathname, pathString);
    if (IS_ERR(status))
    {
        return status;
    }

    file_t* files[2];
    status = vfs_open2(&pathname, files, process);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(files[0]);
    UNREF_DEFER(files[1]);

    fd_t fdsLocal[2];
    fdsLocal[0] = file_table_open(&process->files, files[0]);
    if (fdsLocal[0] == FD_NONE)
    {
        return ERR(VFS, BADFD);
    }
    fdsLocal[1] = file_table_open(&process->files, files[1]);
    if (fdsLocal[1] == FD_NONE)
    {
        file_table_close(&process->files, fdsLocal[0]);
        return ERR(VFS, BADFD);
    }

    status = thread_copy_to_user(thread, fds, fdsLocal, sizeof(fd_t) * 2);
    if (IS_ERR(status))
    {
        file_table_close(&process->files, fdsLocal[0]);
        file_table_close(&process->files, fdsLocal[1]);
        return status;
    }

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

SYSCALL_DEFINE(SYS_READ, fd_t fd, void* buffer, size_t count)
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
    size_t bytesRead = 0;
    status = vfs_read(file, buffer, count, &bytesRead);
    space_unpin(&process->space, buffer, count);
    if (IS_OK(status))
    {
        *_result = bytesRead;
    }
    return status;
}

SYSCALL_DEFINE(SYS_WRITE, fd_t fd, const void* buffer, size_t count)
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
    status = vfs_write(file, buffer, count, &bytesWritten);
    space_unpin(&process->space, buffer, count);
    if (IS_OK(status))
    {
        *_result = bytesWritten;
    }
    return status;
}

SYSCALL_DEFINE(SYS_SEEK, fd_t fd, ssize_t offset, seek_origin_t origin)
{
    process_t* process = process_current();

    file_t* file = file_table_get(&process->files, fd);
    if (file == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(file);

    size_t newPos = 0;
    status_t status = vfs_seek(file, offset, origin, &newPos);
    if (IS_OK(status))
    {
        *_result = newPos;
    }
    return status;
}

SYSCALL_DEFINE(SYS_IOCTL, fd_t fd, uint64_t request, void* argp, size_t size)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    file_t* file = file_table_get(&process->files, fd);
    if (file == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(file);

    status_t status = space_pin(&process->space, argp, size, &thread->userStack);
    if (IS_ERR(status))
    {
        return status;
    }
    uint64_t ioctlResult = 0;
    status = vfs_ioctl(file, request, argp, size, &ioctlResult);
    space_unpin(&process->space, argp, size);
    if (IS_OK(status))
    {
        *_result = ioctlResult;
    }
    return status;
}

SYSCALL_DEFINE(SYS_MMAP, fd_t fd, void* addr, size_t length, prot_t prot)
{
    process_t* process = process_current();
    space_t* space = &process->space;

    if (addr != NULL && IS_ERR(space_check_access(space, addr, length)))
    {
        return ERR(VFS, INVAL);
    }

    pml_flags_t flags = vmm_prot_to_flags(prot);
    if (flags == PML_NONE)
    {
        return ERR(VFS, INVAL);
    }

    file_t* file = file_table_get(&process->files, fd);
    if (file == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(file);

    if ((!(file->mode & MODE_READ) && (prot & PROT_READ)) || (!(file->mode & MODE_WRITE) && (prot & PROT_WRITE)) ||
        (!(file->mode & MODE_EXECUTE) && (prot & PROT_EXECUTE)))
    {
        return ERR(VFS, ACCESS);
    }

    status_t status = vfs_mmap(file, &addr, length, flags | PML_USER);
    if (IS_OK(status))
    {
        *_result = (uint64_t)addr;
    }
    return status;
}

SYSCALL_DEFINE(SYS_POLL, pollfd_t* fds, uint64_t amount, clock_t timeout)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    if (amount == 0 || amount >= CONFIG_MAX_FD)
    {
        return ERR(VFS, INVAL);
    }

    status_t status = space_pin(&process->space, fds, sizeof(pollfd_t) * amount, &thread->userStack);
    if (IS_ERR(status))
    {
        return ERR(VFS, FAULT);
    }

    poll_file_t files[CONFIG_MAX_FD];
    for (uint64_t i = 0; i < amount; i++)
    {
        files[i].file = file_table_get(&process->files, fds[i].fd);
        if (files[i].file == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                UNREF(files[j].file);
            }
            fds[i].revents = POLLNVAL;
            space_unpin(&process->space, fds, sizeof(pollfd_t) * amount);
            return ERR(VFS, BADFD);
        }

        files[i].events = fds[i].events;
        files[i].revents = POLLNONE;
    }

    size_t readyCount = 0;
    status = vfs_poll(files, amount, timeout, &readyCount);
    if (IS_OK(status))
    {
        for (uint64_t i = 0; i < amount; i++)
        {
            fds[i].revents = files[i].revents;
        }
        *_result = readyCount;
    }
    space_unpin(&process->space, fds, sizeof(pollfd_t) * amount);

    for (uint64_t i = 0; i < amount; i++)
    {
        UNREF(files[i].file);
    }

    return status;
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
        return ERR(VFS, INVAL);
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
