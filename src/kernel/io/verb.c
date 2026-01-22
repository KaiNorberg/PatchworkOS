#include <kernel/io/verb.h>
#include <kernel/io/io.h>
#include <kernel/sched/wait.h>
#include <kernel/fs/file_table.h>
#include <kernel/mem/mdl.h>
#include <kernel/proc/process.h>

void verb_args_cleanup(irp_t* irp)
{
    switch (irp->verb)
    {
    case VERB_READ:
    {
        UNREF(irp->file);
        irp->file = NULL;
    }
    break;
    default:
        break;
    }
}

static void verb_args_user(irp_t* irp)
{
    assert(!(irp->flags & SQE_KERNEL));

    process_t* process = irp_get_process(irp);

    switch (irp->verb)
    {
    case VERB_READ:
    {
        file_t* file = file_table_get(&process->fileTable, irp->sqe.fd);
        if (file == NULL)
        {
            irp->err = EBADF;
            return;
        }

        if (mdl_from_region(&irp->mdl, NULL, &process->space, irp->sqe.buffer, irp->sqe.count) == ERR)
        {
            UNREF(file);
            irp->err = EFAULT;
            return;
        }

        irp->file = file;
        irp->buffer = &irp->mdl;
        irp->count = irp->sqe.count;
        irp->offset = irp->sqe.offset;
    }
    break;
    default:
        break;
    }
}

static uint64_t nop_cancel(irp_t* irp)
{
    irp_complete(irp);
    return 0;
}

static void verb_dispatch_file(irp_t* irp)
{
    file_t* file = irp->file;
    assert(file != NULL);

    if (verb_invoke(irp, file->verbs))
    {
        return;
    }

    if (verb_invoke(irp, file->inode->verbs))
    {
        return;
    }

    if (verb_invoke(irp, file->inode->superblock->verbs))
    {
        return;
    }

    irp_error(irp, ENOSYS);
}

void verb_dispatch(irp_t* irp)
{
    if (!(irp->flags & SQE_KERNEL))
    {
        verb_args_user(irp);
    }

    if (irp->err != EINPROGRESS)
    {
        irp_complete(irp);
        return;
    }

    if (irp->timeout != CLOCKS_NEVER)
    {
        irp_timeout_add(irp);
    }

    switch (irp->verb)
    {
    case VERB_NOP:
        irp_set_cancel(irp, nop_cancel);
        break;
    case VERB_READ:
        verb_dispatch_file(irp);
        break;
    default:
        break;
    };
}

static void verb_run_completion(irp_t* irp, void* ctx)
{
    UNUSED(irp);

    wait_queue_t* wait = (wait_queue_t*)ctx;
    wait_unblock(wait, WAIT_ALL, EOK);
}

void verb_run(irp_t* irp)
{
    wait_queue_t wait;
    wait_queue_init(&wait);

    irp_push(irp, verb_run_completion, &wait);
    verb_dispatch(irp);

    WAIT_BLOCK(&wait, irp->err != EINPROGRESS);

    wait_queue_deinit(&wait);
}
