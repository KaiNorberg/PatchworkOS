#include <kernel/fs/vfs.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/binding.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file_table.h>
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
#include <libstd/fs.h>
#include <libstd/list.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static status_t vfs_open_done(irp_t* irp, path_state_t* state, file_t* file)
{
    UNUSED(state);

    irp->result = (uintptr_t)file;
    return OK;
}

status_t vfs_open(file_t** out, const path_t* from, const char* pathname, process_t* process)
{
    if (out == NULL || from == NULL || pathname == NULL || process == NULL)
    {
        return ERR(VFS, INVAL);
    }

    size_t len = strlen(pathname);
    if (len >= MAX_PATH)
    {
        return ERR(VFS, PATHTOOLONG);
    }

    path_state_t* state = malloc(sizeof(path_state_t));
    if (state == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    path_state_init(state, from->dentry, from->binding, NULL, vfs_open_done);

    memcpy(state->path, pathname, len + 1);

    irp_t* irp = irp_new(process, NULL);
    if (irp == NULL)
    {
        free(state);
        return ERR(VFS, NOMEM);
    }

    vfs_sync_ctx_t ctx;
    wait_queue_init(&ctx.wait);
    atomic_init(&ctx.done, false);

    irp_set_complete(irp, vfs_sync_complete, &ctx);

    status_t status = path_walk(irp, state, len);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        return status;
    }

    WAIT_BLOCK(&ctx.wait, atomic_load(&ctx.done));

    status = ctx.status;
    if (IS_ERR(status))
    {
        return status;
    }

    *out = (file_t*)ctx.result;
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

    irp_t* irp = irp_new(process_current(), NULL);
    if (irp == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    sglist_t* list;
    status_t status = irp_get_sglist(irp, &list);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        return status;
    }

    status = sglist_add(list, &irp->process->space, buffer, count);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        return status;
    }

    irp_prep_read(irp, list, IOCUR);
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

    irp_t* irp = irp_new(process_current(), NULL);
    if (irp == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    sglist_t* list;
    status_t status = irp_get_sglist(irp, &list);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        return status;
    }

    status = sglist_add(list, &irp->process->space, buffer, count);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        return status;
    }

    irp_prep_write(irp, list, IOCUR);
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

    irp_t* irp = irp_new(process_current(), NULL);
    if (irp == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    irp_prep_seek(irp, offset, origin);
    uint64_t result = 0;
    status_t status = vfs_run_sync(irp, file, &result);
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