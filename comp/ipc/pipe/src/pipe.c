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
 * Pipes are created using the `/dev/pipe/clone` file. Opening this file will return one file descriptor
 * that can be used for both reading and writing.
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

static dentry_t* dir = NULL;
static dentry_t* clone = NULL;

static status_t pipe_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;

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

static status_t pipe_close(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    pipe_t* data = file->data;
    if (data == NULL)
    {
        return OK;
    }

    free(data);
    file->data = NULL;
    return OK;
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

    if (sglist_size(frame->read.buffer) == 0)
    {
        irp->result = 0;
        return OK;
    }

    LOCK_SCOPE(&data->lock);

    if (fifo_bytes_readable(&data->fifo) == 0)
    {
        return irp_delay(irp, &data->readers, pipe_cancel);
    }

    status_t status = fifo_read_sglist(&data->fifo, frame->read.buffer, SIZE_MAX, 0, &irp->result);

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
        irp_complete(writer, fifo_write_sglist(&data->fifo, writerFrame->write.buffer, SIZE_MAX, 0, &writer->result));
    }

    irp_t* poll;
    LIST_FOR_EACH_SAFE(poll, temp, &data->polls, entry)
    {
        irp_frame_t* pollFrame = irp_current(poll);

        ioevents_t events = 0;
        if (fifo_bytes_readable(&data->fifo) > 0)
        {
            events |= IOEVENT_READ;
        }
        if (fifo_bytes_writeable(&data->fifo) > 0)
        {
            events |= IOEVENT_WRITE;
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

    size_t count = sglist_size(frame->write.buffer);
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

    status_t status = fifo_write_sglist(&data->fifo, frame->write.buffer, SIZE_MAX, 0, &irp->result);

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
        irp_complete(reader, fifo_read_sglist(&data->fifo, readerFrame->read.buffer, SIZE_MAX, 0, &reader->result));
    }

    irp_t* poll;
    LIST_FOR_EACH_SAFE(poll, temp, &data->polls, entry)
    {
        irp_frame_t* pollFrame = irp_current(poll);

        ioevents_t events = 0;
        if (fifo_bytes_readable(&data->fifo) > 0)
        {
            events |= IOEVENT_READ;
        }
        if (fifo_bytes_writeable(&data->fifo) > 0)
        {
            events |= IOEVENT_WRITE;
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
        irp->result |= IOEVENT_READ;
    }
    if (fifo_bytes_writeable(&data->fifo) > 0)
    {
        irp->result |= IOEVENT_WRITE;
    }

    if (irp->result & frame->poll.events)
    {
        return OK;
    }

    return irp_delay(irp, &data->polls, pipe_cancel);
}

static vnode_class_t pipeClass = {
    .name = "pipe",
    .type = FILE_TYPE_REGULAR,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = pipe_open,
            [IRP_MJ_CLOSE] = pipe_close,
            [IRP_MJ_READ] = pipe_read,
            [IRP_MJ_WRITE] = pipe_write,
            [IRP_MJ_POLL] = pipe_poll,
        },
};

static vnode_class_t dirClass = {
    .name = "pipe",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

status_t pipe_init(void)
{
    dir = devfs_dentry_new(NULL, "pipe", &dirClass, NULL);
    if (dir == NULL)
    {
        LOG_ERR("failed to initialize pipe directory");
        return ERR(DRIVER, IO);
    }

    clone = devfs_dentry_new(dir, "clone", &pipeClass, NULL);
    if (clone == NULL)
    {
        UNREF(dir);
        LOG_ERR("failed to initialize pipe new file");
        return ERR(DRIVER, IO);
    }

    return OK;
}

void pipe_deinit(void)
{
    UNREF(clone);
    clone = NULL;
    UNREF(dir);
    dir = NULL;
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