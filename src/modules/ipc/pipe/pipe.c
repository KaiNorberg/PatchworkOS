#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/module/module.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/fifo.h>

#include <assert.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/io.h>
#include <sys/math.h>

/**
 * @brief Pipes.
 * @defgroup kernel_ipc_pipe Pipes
 * @ingroup kernel_ipc
 *
 * Pipes are exposed in the `/dev/pipe` directory. Pipes are two way communication channels that can be used for
 * inter process communication (IPC).
 *
 * ## Creating Pipes
 *
 * Pipes are created using the `/dev/pipe/new` file. Opening this file using `open()` will return one file descriptor
 * that can be used for both reading and writing.
 *
 * ## Using Pipes
 *
 * Pipes can be read from and written to using the expected `read()` and `write()` system calls. Pipes are blocking and
 * pollable, following expected POSIX semantics.
 *
 * @{
 */

typedef struct
{
    fifo_t fifo;
    list_t readers;
    list_t writers;
    list_t polls;
    lock_t lock;
    uint8_t buffer[PAGE_SIZE - sizeof(lock_t) - (sizeof(list_t) * 3) - sizeof(fifo_t)];
} pipe_t;

static dentry_t* pipeDir = NULL;
static dentry_t* newFile = NULL;

static status_t pipe_open(file_t* file)
{
    pipe_t* data = malloc(sizeof(pipe_t));
    if (data == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }
    fifo_init(&data->fifo, data->buffer, ARRAY_SIZE(data->buffer));
    list_init(&data->readers);
    list_init(&data->writers);
    list_init(&data->polls);
    lock_init(&data->lock);

    file->data = data;
    return OK;
}

static void pipe_close(file_t* file)
{
    pipe_t* data = file->data;
    if (data == NULL)
    {
        return;
    }

    free(data);
    file->data = NULL;
}

static status_t pipe_cancel(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file == NULL)
    {
        return ERR(DRIVER, EXPECT_FILE);
    }

    pipe_t* data = file->data;
    assert(data != NULL);

    lock_acquire(&data->lock);

    if (list_contains(&irp->entry))
    {
        list_remove(&irp->entry);
    }

    lock_release(&data->lock);

    return OK;
}

static status_t pipe_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file == NULL)
    {
        return ERR(DRIVER, EXPECT_FILE);
    }

    pipe_t* data = file->data;

    if (mdl_size(frame->read.buffer) == 0)
    {
        irp->result = 0;
        return OK;
    }

    LOCK_SCOPE(&data->lock);

    if (fifo_bytes_readable(&data->fifo) == 0)
    {
        return irp_delay(irp, &data->readers, pipe_cancel);
    }

    status_t status = fifo_read_mdl(&data->fifo, frame->read.buffer, 0, &irp->result);

    irp_t* writer;
    irp_t* temp;
    LIST_FOR_EACH_SAFE(writer, temp, &data->writers, entry)
    {
        if (fifo_bytes_writeable(&data->fifo) == 0)
        {
            break;
        }

        if (!irp_claim(writer))
        {
            continue;
        }
        list_remove(&writer->entry);

        irp_frame_t* writerFrame = irp_current(writer);
        irp_complete(writer, fifo_write_mdl(&data->fifo, writerFrame->write.buffer, 0, &writer->result));
    }

    irp_t* poll;
    LIST_FOR_EACH_SAFE(poll, temp, &data->polls, entry)
    {
        irp_frame_t* pollFrame = irp_current(poll);

        ioevents_t events = 0;
        if (fifo_bytes_readable(&data->fifo) > 0)
        {
            events |= IOPOLL_READ;
        }
        if (fifo_bytes_writeable(&data->fifo) > 0)
        {
            events |= IOPOLL_WRITE;
        }

        if (!(pollFrame->poll.events & events))
        {
            continue;
        }

        if (!irp_claim(poll))
        {
            continue;
        }
        list_remove(&poll->entry);

        poll->result = events & pollFrame->poll.events;
        irp_complete(poll, OK);
    }

    return status;
}

static status_t pipe_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file == NULL)
    {
        return ERR(DRIVER, EXPECT_FILE);
    }

    pipe_t* data = file->data;

    size_t count = mdl_size(frame->write.buffer);
    if (count == 0)
    {
        irp->result = 0;
        return OK;
    }

    LOCK_SCOPE(&data->lock);

    if (fifo_bytes_writeable(&data->fifo) == 0)
    {
        return irp_delay(irp, &data->writers, pipe_cancel);
    }

    status_t status = fifo_write_mdl(&data->fifo, frame->write.buffer, 0, &irp->result);

    irp_t* reader;
    irp_t* temp;
    LIST_FOR_EACH_SAFE(reader, temp, &data->readers, entry)
    {
        if (fifo_bytes_readable(&data->fifo) == 0)
        {
            break;
        }

        if (!irp_claim(reader))
        {
            continue;
        }
        list_remove(&reader->entry);

        irp_frame_t* readerFrame = irp_current(reader);
        irp_complete(reader, fifo_read_mdl(&data->fifo, readerFrame->read.buffer, 0, &reader->result));
    }

    irp_t* poll;
    LIST_FOR_EACH_SAFE(poll, temp, &data->polls, entry)
    {
        irp_frame_t* pollFrame = irp_current(poll);

        ioevents_t events = 0;
        if (fifo_bytes_readable(&data->fifo) > 0)
        {
            events |= IOPOLL_READ;
        }
        if (fifo_bytes_writeable(&data->fifo) > 0)
        {
            events |= IOPOLL_WRITE;
        }

        if (!(pollFrame->poll.events & events))
        {
            continue;
        }

        if (!irp_claim(poll))
        {
            continue;
        }
        list_remove(&poll->entry);

        poll->result = events & pollFrame->poll.events;
        irp_complete(poll, OK);
    }

    return status;
}

static status_t pipe_poll(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file == NULL)
    {
        return ERR(DRIVER, EXPECT_FILE);
    }

    pipe_t* data = file->data;

    LOCK_SCOPE(&data->lock);

    irp->result = 0;
    if (fifo_bytes_readable(&data->fifo) > 0)
    {
        irp->result |= IOPOLL_READ;
    }
    if (fifo_bytes_writeable(&data->fifo) > 0)
    {
        irp->result |= IOPOLL_WRITE;
    }

    if (irp->result & frame->poll.events)
    {
        return OK;
    }
    
    return irp_delay(irp, &data->polls, pipe_cancel);
}

static vnode_class_t pipeClass = {
    .name = "pipe",
    .type = VNODE_REGULAR,
    .open = pipe_open,
    .close = pipe_close,
    .handlers =
        {
            [IRP_MJ_READ] = pipe_read,
            [IRP_MJ_WRITE] = pipe_write,
            [IRP_MJ_POLL] = pipe_poll,
        },
};

static vnode_class_t dirClass = {
    .name = "pipe",
    .type = VNODE_DIR,
    .iterate = dentry_generic_iterate,
};

status_t pipe_init(void)
{
    pipeDir = devfs_dentry_new(NULL, "pipe", &dirClass, NULL);
    if (pipeDir == NULL)
    {
        LOG_ERR("failed to initialize pipe directory");
        return ERR(DRIVER, IO);
    }

    newFile = devfs_dentry_new(pipeDir, "new", &pipeClass, NULL);
    if (newFile == NULL)
    {
        UNREF(pipeDir);
        LOG_ERR("failed to initialize pipe new file");
        return ERR(DRIVER, IO);
    }

    return OK;
}

void pipe_deinit(void)
{
    UNREF(newFile);
    newFile = NULL;
    UNREF(pipeDir);
    pipeDir = NULL;
}

/** @} */

status_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        return pipe_init();
    case MODULE_EVENT_UNLOAD:
        pipe_deinit();
        break;
    default:
        break;
    }

    return OK;
}

MODULE_INFO("Pipes", "Kai Norberg", "Implements pipes for inter-process communication", OS_VERSION, "MIT",
    "BOOT_ALWAYS");