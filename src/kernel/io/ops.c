#include <_libstd/MAX_PATH.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/ioring.h>
#include <kernel/io/irp.h>
#include <kernel/mem/paging_types.h>
#include <kernel/proc/process.h>

#include <sys/io.h>

static status_t io_op_cancel(irp_t* irp)
{
    ioring_ctx_t* ctx = irp_get_ctx(irp);
    size_t count = 0;
    for (size_t i = 0; i < ctx->irps->size; i++)
    {
        irp_t* target = &ctx->irps->irps[i];
        if (target == irp)
        {
            continue;
        }

        if (target->sqe.data != irp->sqe.target && !(irp->sqe.cancel & IOCANCEL_ANY))
        {
            continue;
        }

        if (irp_cancel(target) == EOK)
        {
            count++;
            if (!(irp->sqe.cancel & IOCANCEL_ALL))
            {
                break;
            }
        }
    }

    irp->result = count;
    return OK;
}

static status_t io_op_read(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    if (!(file->mode & MODE_READ))
    {
        return ERR(IO, ACCESS);
    }

    mdl_t* mdl;
    status_t status = irp_get_mdl(irp, &mdl);
    if (IS_ERR(status))
    {
        return status;
    }

    status = mdl_add_vector(mdl, &process->space, irp->sqe.vector, irp->sqe.count);
    if (IS_ERR(status))
    {
        return status;
    }

    irp_prep_read(irp, mdl, irp->sqe.offset);
    return file_call(file, irp);
}

static status_t io_op_write(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    if (!(file->mode & MODE_WRITE))
    {
        return ERR(IO, ACCESS);
    }

    mdl_t* mdl;
    status_t status = irp_get_mdl(irp, &mdl);
    if (IS_ERR(status))
    {
        return status;
    }

    status = mdl_add_vector(mdl, &process->space, irp->sqe.vector, irp->sqe.count);
    if (IS_ERR(status))
    {
        return status;
    }

    irp_prep_write(irp, mdl, irp->sqe.offset);
    return file_call(file, irp);
}

static status_t io_op_poll(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    irp_prep_poll(irp, irp->sqe.events);
    return file_call(file, irp);
}

static status_t io_op_seek(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    irp_prep_seek(irp, irp->sqe.offset, irp->sqe.origin);
    return file_call(file, irp);
}

static status_t io_op_map(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    pml_flags_t pml = vmm_iomem_to_flags(irp->sqe.mem);
    irp_prep_mmap(irp, irp->sqe.address, irp->sqe.count, irp->sqe.offset, pml);
    return file_call(file, irp);
}

static status_t io_op_walk_done(irp_t* irp, struct path_state* state, file_t* file)
{
    UNUSED(state);

    return file_table_open(&irp_get_process(irp)->files, file);
}

static status_t io_op_walk(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    if (irp->sqe.path == NULL || irp->sqe.count >= MAX_PATH)
    {
        return ERR(IO, INVAL);
    }

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    size_t length = sizeof(path_state_t) + MAX_PATH;
    path_state_t* state = malloc(length);
    if (state == NULL)
    {
        return ERR(IO, NOMEM);
    }
    char* path = (char*)((uintptr_t)state + sizeof(path_state_t));

    status_t status = space_copy_out(&process->space, path, irp->sqe.path, irp->sqe.count);
    if (IS_ERR(status))
    {
        free(state);
        return status;
    }

    path_state_init(state, file->path.dentry, file->path.mount, path, irp->sqe.count, MAX_PATH, io_op_walk_done);
    return path_walk(irp, state);
}

static status_t io_op_close(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    return file_table_close(&process->files, irp->sqe.fd);
}

static status_t io_op_remove(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    dentry_t* dentry = file->path.dentry;
    if (DENTRY_IS_ROOT(dentry))
    {
        return ERR(IO, BUSY);
    }

    irp_prep_remove(irp, dentry);
    return file_call(file, irp);
}

static status_t io_op_attr(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    irp_prep_attr(irp, irp->sqe.attr, irp->sqe.value);
    return file_call(file, irp);
}

static status_t io_op_query(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    mdl_t* mdl;
    status_t status = irp_get_mdl(irp, &mdl);
    if (IS_ERR(status))
    {
        return status;
    }

    status = mdl_add(mdl, &process->space, irp->sqe.info, sizeof(file_info_t));
    if (IS_ERR(status))
    {
        return status;
    }

    irp_prep_query(irp, mdl);
    return file_call(file, irp);
}

static status_t io_op_flush(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    irp_prep_flush(irp);
    return file_call(file, irp);
}

typedef status_t (*io_op_func_t)(irp_t*);

static const io_op_func_t ops[IOOP_MAX] = {
    [IOOP_CANCEL] = io_op_cancel,
    [IOOP_READ] = io_op_read,
    [IOOP_WRITE] = io_op_write,
    [IOOP_POLL] = io_op_poll,
    [IOOP_SEEK] = io_op_seek,
    [IOOP_MAP] = io_op_map,
    [IOOP_WALK] = io_op_walk,
    [IOOP_CLOSE] = io_op_close,
    [IOOP_REMOVE] = io_op_remove,
    [IOOP_ATTR] = io_op_attr,
    [IOOP_QUERY] = io_op_query,
    [IOOP_FLUSH] = io_op_flush,
};

status_t io_op_dispatch(irp_t* irp)
{
    ioring_ctx_t* ctx = irp_get_ctx(irp);
    ioring_t* ring = &ctx->ring;

    iosqe_flags_t reg = (irp->sqe.flags >> IOSQE_LOAD0) & IOSQE_REG_MASK;
    if (reg != IOSQE_REG_NONE)
    {
        irp->sqe.arg0 = atomic_load_explicit(&ring->ctrl->regs[reg - 1], memory_order_acquire);
    }

    reg = (irp->sqe.flags >> IOSQE_LOAD1) & IOSQE_REG_MASK;
    if (reg != IOSQE_REG_NONE)
    {
        irp->sqe.arg1 = atomic_load_explicit(&ring->ctrl->regs[reg - 1], memory_order_acquire);
    }

    reg = (irp->sqe.flags >> IOSQE_LOAD2) & IOSQE_REG_MASK;
    if (reg != IOSQE_REG_NONE)
    {
        irp->sqe.arg2 = atomic_load_explicit(&ring->ctrl->regs[reg - 1], memory_order_acquire);
    }

    reg = (irp->sqe.flags >> IOSQE_LOAD3) & IOSQE_REG_MASK;
    if (reg != IOSQE_REG_NONE)
    {
        irp->sqe.arg3 = atomic_load_explicit(&ring->ctrl->regs[reg - 1], memory_order_acquire);
    }

    reg = (irp->sqe.flags >> IOSQE_LOAD4) & IOSQE_REG_MASK;
    if (reg != IOSQE_REG_NONE)
    {
        irp->sqe.arg4 = atomic_load_explicit(&ring->ctrl->regs[reg - 1], memory_order_acquire);
    }

    if (irp->sqe.op >= ARRAY_SIZE(ops) || ops[irp->sqe.op] == NULL)
    {
        return ERR(IO, INVAL);
    }

    return ops[irp->sqe.op](irp);
}