#include <kernel/fs/file_table.h>
#include <kernel/io/ioring.h>
#include <kernel/io/irp.h>
#include <kernel/proc/process.h>

#include <sys/ioring.h>

static status_t nop_cancel(irp_t* irp)
{
    UNUSED(irp);

    return OK;
}

static void io_op_nop(irp_t* irp)
{
    irp_set_cancel(irp, nop_cancel);
    irp_timeout_add(irp, irp->sqe.timeout);
}

static void io_op_cancel(irp_t* irp)
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

        if (target->sqe.data != irp->sqe.target && !(irp->sqe.cancel & IO_CANCEL_ANY))
        {
            continue;
        }

        if (irp_cancel(target) == EOK)
        {
            count++;
        }

        if (!(irp->sqe.cancel & IO_CANCEL_ALL))
        {
            break;
        }
    }

    irp->res._raw = count;
    irp_complete(irp, OK);
}

static void io_op_read(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        irp_complete(irp, EBADF);
        return;
    }

    mdl_t* mdl;
    status_t status = irp_get_mdl(irp, &mdl, irp->sqe.buffer, irp->sqe.count);
    if (IS_ERR(status))
    {
        UNREF(file);
        irp_complete(irp, status);
        return;
    }

    irp_prep_read(irp, file, mdl, irp->sqe.count, irp->sqe.offset == IO_OFF_CUR ? file->pos : (size_t)irp->sqe.offset);
    irp_call(irp, file->vnode);
}

static void io_op_write(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        irp_complete(irp, EBADF);
        return;
    }

    mdl_t* mdl;
    status_t status = irp_get_mdl(irp, &mdl, irp->sqe.buffer, irp->sqe.count);
    if (IS_ERR(status))
    {
        UNREF(file);
        irp_complete(irp, status);
        return;
    }

    irp_prep_write(irp, file, mdl, irp->sqe.count, irp->sqe.offset == IO_OFF_CUR ? file->pos : (size_t)irp->sqe.offset);
    irp_call(irp, file->vnode);
}

static void io_op_poll(irp_t* irp)
{
    process_t* process = irp_get_process(irp);

    file_t* file = file_table_get(&process->files, irp->sqe.fd);
    if (file == NULL)
    {
        irp_complete(irp, ERR(IO, INVAL));
        return;
    }

    irp_prep_poll(irp, file, irp->sqe.events);
    irp_call(irp, file->vnode);
}

typedef void (*io_op_func_t)(irp_t*);

static const io_op_func_t ops[IO_OP_MAX] = {
    [IO_OP_NOP] = io_op_nop,
    [IO_OP_CANCEL] = io_op_cancel,
    [IO_OP_READ] = io_op_read,
    [IO_OP_WRITE] = io_op_write,
    [IO_OP_POLL] = io_op_poll,
};

void io_op_dispatch(irp_t* irp)
{
    ioring_ctx_t* ctx = irp_get_ctx(irp);
    ioring_t* ring = &ctx->ring;

    sqe_flags_t reg = (irp->sqe.flags >> SQE_LOAD0) & SQE_REG_MASK;
    if (reg != SQE_REG_NONE)
    {
        irp->sqe.arg0 = atomic_load_explicit(&ring->ctrl->regs[reg - 1], memory_order_acquire);
    }

    reg = (irp->sqe.flags >> SQE_LOAD1) & SQE_REG_MASK;
    if (reg != SQE_REG_NONE)
    {
        irp->sqe.arg1 = atomic_load_explicit(&ring->ctrl->regs[reg - 1], memory_order_acquire);
    }

    reg = (irp->sqe.flags >> SQE_LOAD2) & SQE_REG_MASK;
    if (reg != SQE_REG_NONE)
    {
        irp->sqe.arg2 = atomic_load_explicit(&ring->ctrl->regs[reg - 1], memory_order_acquire);
    }

    reg = (irp->sqe.flags >> SQE_LOAD3) & SQE_REG_MASK;
    if (reg != SQE_REG_NONE)
    {
        irp->sqe.arg3 = atomic_load_explicit(&ring->ctrl->regs[reg - 1], memory_order_acquire);
    }

    reg = (irp->sqe.flags >> SQE_LOAD4) & SQE_REG_MASK;
    if (reg != SQE_REG_NONE)
    {
        irp->sqe.arg4 = atomic_load_explicit(&ring->ctrl->regs[reg - 1], memory_order_acquire);
    }

    if (irp->sqe.op >= ARRAY_SIZE(ops) || ops[irp->sqe.op] == NULL)
    {
        irp_complete(irp, ERR(IO, INVAL));
        return;
    }

    ops[irp->sqe.op](irp);
}