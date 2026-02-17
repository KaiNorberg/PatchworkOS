#include <kernel/fs/file_table.h>
#include <kernel/fs/path.h>
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

    if (!(file->mode & MODE_READ))
    {
        UNREF(file);
        return ERR(IO, ACCESS);
    }

    mdl_t* mdl;
    status_t status = irp_get_mdl(irp, &mdl);
    if (IS_ERR(status))
    {
        UNREF(file);
        return status;
    }

    status = mdl_add_vector(mdl, &process->space, irp->sqe.vector, irp->sqe.count);
    if (IS_ERR(status))
    {
        UNREF(file);
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

    if (!(file->mode & MODE_WRITE))
    {
        UNREF(file);
        return ERR(IO, ACCESS);
    }

    mdl_t* mdl;
    status_t status = irp_get_mdl(irp, &mdl);
    if (IS_ERR(status))
    {
        UNREF(file);
        return status;
    }

    status = mdl_add_vector(mdl, &process->space, irp->sqe.vector, irp->sqe.count);
    if (IS_ERR(status))
    {
        UNREF(file);
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

    irp_prep_seek(irp, irp->sqe.offset, irp->sqe.origin);
    return file_call(file, irp);
}

static status_t io_op_mmap(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }

    pml_flags_t pml = vmm_iomem_to_flags(irp->sqe.mem);
    irp_prep_mmap(irp, irp->sqe.address, irp->sqe.count, irp->sqe.offset, pml);
    return file_call(file, irp);
}

static status_t io_op_control(irp_t* irp)
{
    UNUSED(irp);

    /// @todo Implement `IOOP_CONTROL`.
    return ERR(IO, INVAL);
}

static status_t io_op_walk_complete(irp_t* irp, void* ctx)
{
    free(ctx);
    return file_table_open(&irp_get_process(irp)->files, irp_next(irp)->file);
}

static status_t io_op_walk(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    if (irp->sqe.path == NULL || (irp->sqe.extra == NULL && irp->sqe.extraLen != 0))
    {
        return ERR(IO, INVAL);
    }

    path_t from = PATH_EMPTY;
    if (irp->sqe.fd == IOCWD)
    {
        namespace_t* ns = process_get_ns(process);
        if (ns == NULL)
        {
            return ERR(IO, DYING);
        }
        UNREF_DEFER(ns);

        from = cwd_get(&process->cwd, ns);
    }
    else
    {
        file_t* file = file_table_get(&process->files, irp->sqe.fd);
        if (file == NULL)
        {
            return ERR(IO, BADFD);
        }

        path_copy(&from, &file->path);
    }

    size_t length = irp->sqe.count + 1 + irp->sqe.extraLen;
    char* path = malloc(length);
    if (path == NULL)
    {
        return ERR(IO, NOMEM);
    }
    void* extra = path + irp->sqe.count + 1;

    status_t status = space_copy_out(&process->space, path, irp->sqe.path, irp->sqe.count);
    if (IS_ERR(status))
    {
        free(path);
        return status;
    }

    status = space_copy_out(&process->space, extra, irp->sqe.extra, irp->sqe.extraLen);
    if (IS_ERR(status))
    {
        free(path);
        return status;
    }

    irp_set_complete(irp, io_op_walk_complete, path);    
    irp_prep_open(irp, path, extra, irp->sqe.extraLen);

    return path_call(&from, irp);
}

typedef status_t (*io_op_func_t)(irp_t*);

static const io_op_func_t ops[IOOP_MAX] = {
    [IOOP_CANCEL] = io_op_cancel,
    [IOOP_READ] = io_op_read,
    [IOOP_WRITE] = io_op_write,
    [IOOP_POLL] = io_op_poll,
    [IOOP_SEEK] = io_op_seek,
    [IOOP_MAP] = io_op_mmap,
    [IOOP_WALK] = io_op_walk,
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