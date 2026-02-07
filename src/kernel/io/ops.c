#include <kernel/fs/file_table.h>
#include <kernel/io/ioring.h>
#include <kernel/io/irp.h>
#include <kernel/mem/paging_types.h>
#include <kernel/proc/process.h>

#include <sys/ioring.h>

static status_t nop_cancel(irp_t* irp)
{
    UNUSED(irp);

    return OK;
}

static status_t io_op_nop(irp_t* irp)
{
    irp_set_cancel(irp, nop_cancel);
    irp_timeout_add(irp);
    return OK;
}

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
        }

        if (!(irp->sqe.cancel & IOCANCEL_ALL))
        {
            break;
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
    status_t status = irp_get_mdl(irp, &mdl, irp->sqe.buffer, irp->sqe.count);
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
    status_t status = irp_get_mdl(irp, &mdl, irp->sqe.buffer, irp->sqe.count);
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

    pml_flags_t pml = 0;
    if (irp->sqe.mmap & IOMAP_READ)
    {
        pml |= PML_PRESENT | PML_USER;
    }
    if (irp->sqe.mmap & IOMAP_WRITE)
    {
        pml |= PML_WRITE;
    }
    if (!(irp->sqe.mmap & IOMAP_EXEC))
    {
        pml |= PML_NO_EXECUTE;
    }

    irp_prep_mmap(irp, irp->sqe.address, irp->sqe.count, irp->sqe.offset, pml);
    return file_call(file, irp);
}

static status_t io_op_control(irp_t* irp)
{
    UNUSED(irp);

    /// @todo Implement `IOOP_CONTROL`.
    return ERR(IO, INVAL);
}

typedef status_t (*io_op_func_t)(irp_t*);

static const io_op_func_t ops[IOOP_MAX] = {
    [IOOP_NOP] = io_op_nop,
    [IOOP_CANCEL] = io_op_cancel,
    [IOOP_READ] = io_op_read,
    [IOOP_WRITE] = io_op_write,
    [IOOP_POLL] = io_op_poll,
    [IOOP_SEEK] = io_op_seek,
    [IOOP_MMAP] = io_op_mmap,
    [IOOP_CONTROL] = io_op_control,
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