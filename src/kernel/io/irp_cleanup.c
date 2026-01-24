#include <kernel/io/irp.h>

static void irp_cleanup_read(irp_frame_t* frame)
{
    if (frame->read.file != NULL)
    {
        UNREF(frame->read.file);
        frame->read.file = NULL;
    }
}

static void irp_cleanup_write(irp_frame_t* frame)
{
    if (frame->write.file != NULL)
    {
        UNREF(frame->write.file);
        frame->write.file = NULL;
    }
}

static void irp_cleanup_poll(irp_frame_t* frame)
{
    if (frame->poll.file != NULL)
    {
        UNREF(frame->poll.file);
        frame->poll.file = NULL;
    }
}

typedef void (*irp_cleanup_func_t)(irp_frame_t* frame);

static const irp_cleanup_func_t cleanups[IRP_MJ_MAX] = {
    [IRP_MJ_READ] = irp_cleanup_read,
    [IRP_MJ_WRITE] = irp_cleanup_write,
    [IRP_MJ_POLL] = irp_cleanup_poll,
};

void irp_cleanup_args(irp_frame_t* frame)
{
    if (frame->major >= IRP_MJ_MAX)
    {
        return;
    }

    irp_cleanup_func_t func = cleanups[frame->major];
    if (func != NULL)
    {
        func(frame);
    }
}
