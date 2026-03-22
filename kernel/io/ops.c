#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/ioring.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/mem/paging_types.h>
#include <kernel/proc/process.h>

#include <sys/io.h>

static status_t io_op_cancel(irp_t* irp)
{
    ioring_ctx_t* ctx = irp->ctx;
    size_t count = 0;

    while (true)
    {
        bool found = false;
        lock_acquire(&ctx->lock);
        irp_t* target;
        LIST_FOR_EACH(target, &ctx->active, activeEntry)
        {
            if (target == irp)
            {
                continue;
            }

            if (target->sqe.data != irp->sqe.target && !(irp->sqe.cancel & IOCANCEL_ANY))
            {
                continue;
            }

            irp_cancel_t handler = irp_cancel_claim(target);
            if (handler != NULL)
            {
                lock_release(&ctx->lock);

                irp_cancel_finish(target, handler);
                count++;
                found = true;
                break;
            }
        }

        if (!found)
        {
            lock_release(&ctx->lock);
            break;
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
    file_t* file = file_table_get(&irp->process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    if (!(file->mode & MODE_READ))
    {
        return ERR(IO, ACCESS);
    }

    sglist_t* list;
    status_t status = irp_get_sglist(irp, &list);
    if (IS_ERR(status))
    {
        return status;
    }

    status = sglist_add_vector(list, &irp->process->space, irp->sqe.vector, irp->sqe.count);
    if (IS_ERR(status))
    {
        return status;
    }

    irp_prep_read(irp, list, irp->sqe.offset);
    return file_call(file, irp);
}

static status_t io_op_write(irp_t* irp)
{
    file_t* file = file_table_get(&irp->process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    if (!(file->mode & MODE_WRITE))
    {
        return ERR(IO, ACCESS);
    }

    sglist_t* list;
    status_t status = irp_get_sglist(irp, &list);
    if (IS_ERR(status))
    {
        return status;
    }

    status = sglist_add_vector(list, &irp->process->space, irp->sqe.vector, irp->sqe.count);
    if (IS_ERR(status))
    {
        return status;
    }

    irp_prep_write(irp, list, irp->sqe.offset);
    return file_call(file, irp);
}

static status_t io_op_poll(irp_t* irp)
{
    file_t* file = file_table_get(&irp->process->files, irp->sqe.fd);
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
    file_t* file = file_table_get(&irp->process->files, irp->sqe.fd);
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
    file_t* file = file_table_get(&irp->process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    pml_flags_t pml = vmm_iomap_to_flags(irp->sqe.map);
    irp_prep_mmap(irp, irp->sqe.address, irp->sqe.count, irp->sqe.offset, pml);
    return file_call(file, irp);
}

static status_t io_op_walk_done(irp_t* irp, struct path_state* state, file_t* file)
{
    UNUSED(state);

    irp->result = FDNONE;
    return file_table_grab(&irp->process->files, file, &irp->result);
}

static status_t io_op_walk(irp_t* irp)
{
    if (irp->sqe.path == NULL || irp->sqe.pathLen >= MAX_PATH)
    {
        return ERR(IO, INVAL);
    }

    file_t* from = file_table_get(&irp->process->files, irp->sqe.cwd);
    if (from == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(from);

    file_t* root = file_table_get(&irp->process->files, irp->sqe.root);
    if (root == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(root);

    path_state_t* state = malloc(sizeof(path_state_t));
    if (state == NULL)
    {
        return ERR(IO, NOMEM);
    }

    if (irp->sqe.pathLen >= MAX_PATH)
    {
        free(state);
        return ERR(IO, INVAL);
    }

    status_t status = space_copy_out(&irp->process->space, state->path, irp->sqe.path, irp->sqe.pathLen);
    if (IS_ERR(status))
    {
        free(state);
        return status;
    }

    path_state_init(state, from->path.dentry, from->path.binding, root, io_op_walk_done);
    return path_walk(irp, state, irp->sqe.pathLen);
}

static status_t io_op_drop(irp_t* irp)
{
    return file_table_drop(&irp->process->files, irp->sqe.fd);
}

static status_t io_op_remove(irp_t* irp)
{
    file_t* file = file_table_get(&irp->process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    dentry_t* dentry = file->path.dentry;
    if (dentry->parent == NULL)
    {
        return ERR(IO, BUSY);
    }

    irp_prep_remove(irp, dentry);
    return file_call(file, irp);
}

static status_t io_op_attr(irp_t* irp)
{
    file_t* file = file_table_get(&irp->process->files, irp->sqe.fd);
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
    file_t* file = file_table_get(&irp->process->files, irp->sqe.fd);
    if (file == NULL)
    {
        return ERR(IO, BADFD);
    }
    UNREF_DEFER(file);

    sglist_t* list;
    status_t status = irp_get_sglist(irp, &list);
    if (IS_ERR(status))
    {
        return status;
    }

    status = sglist_add(list, &irp->process->space, irp->sqe.info, sizeof(file_info_t));
    if (IS_ERR(status))
    {
        return status;
    }

    irp_prep_query(irp, list);
    return file_call(file, irp);
}

static status_t io_op_flush(irp_t* irp)
{
    file_t* file = file_table_get(&irp->process->files, irp->sqe.fd);
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
    [IOOP_DROP] = io_op_drop,
    [IOOP_REMOVE] = io_op_remove,
    [IOOP_ATTR] = io_op_attr,
    [IOOP_QUERY] = io_op_query,
    [IOOP_FLUSH] = io_op_flush,
};

status_t io_op_dispatch(irp_t* irp)
{
    ioring_ctx_t* ctx = irp->ctx;
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
