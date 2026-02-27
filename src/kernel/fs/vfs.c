#include <_libstd/MAX_PATH.h>
#include <kernel/fs/vfs.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/cwd.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/key.h>
#include <kernel/fs/binding.h>
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

status_t vfs_open(file_t** out, const path_t* from, const pathname_t* pathname, process_t* process)
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

    if (file->vnode->cls->open != NULL)
    {
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        status = file->vnode->cls->open(file);
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
    status = irp_get_mdl(irp, &mdl);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        irp_pool_free(pool);
        return status;
    }

    status = mdl_add(mdl, &irp_get_process(irp)->space, buffer, count);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        irp_pool_free(pool);
        return status;
    }

    irp_prep_read(irp, mdl, IOCUR);
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
    status = irp_get_mdl(irp, &mdl);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        irp_pool_free(pool);
        return status;
    }

    status = mdl_add(mdl, &irp_get_process(irp)->space, buffer, count);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        irp_pool_free(pool);
        return status;
    }

    irp_prep_write(irp, mdl, IOCUR);
    uint64_t result = 0;
    status = vfs_run_sync(irp, file, &result);
    if (out != NULL)
    {
        *out = result;
    }
    return status;
}

status_t vfs_seek(file_t* file, ssize_t offset, ioseek_t origin, size_t* out)
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

uint64_t vfs_id_get(void)
{
    static _Atomic(uint64_t) newVfsId = ATOMIC_VAR_INIT(0);

    return atomic_fetch_add(&newVfsId, 1);
}