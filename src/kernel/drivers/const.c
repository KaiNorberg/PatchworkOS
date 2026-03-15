#include <kernel/fs/devfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>

#include <stdint.h>
#include <string.h>
#include <sys/io.h>
#include <sys/status.h>

static dentry_t* constDir;
static dentry_t* oneFile;
static dentry_t* zeroFile;
static dentry_t* nullFile;

static status_t const_one_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    return sglist_fill(frame->read.buffer, SIZE_MAX, 0, &irp->result, UINT8_MAX);
}

static status_t const_one_mmap(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    void* addr = frame->mmap.address;
    size_t length = frame->mmap.length;
    pml_flags_t flags = frame->mmap.flags;

    status_t status = vmm_alloc(&process_current()->space, &addr, length, PAGE_SIZE, flags, VMM_ALLOC_OVERWRITE);
    if (IS_ERR(status))
    {
        return status;
    }

    memset(addr, -1, length);
    irp->result = (uintptr_t)addr;
    return OK;
}

static vnode_class_t oneClass = {
    .name = "const one",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = const_one_read,
            [IRP_MJ_MMAP] = const_one_mmap,
        },
};

static status_t const_zero_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    return sglist_fill(frame->read.buffer, SIZE_MAX, 0, &irp->result, 0);
}

static status_t const_zero_mmap(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    void* addr = frame->mmap.address;
    size_t length = frame->mmap.length;
    pml_flags_t flags = frame->mmap.flags;

    status_t status = vmm_alloc(&process_current()->space, &addr, length, PAGE_SIZE, flags, VMM_ALLOC_ZERO);
    if (IS_ERR(status))
    {
        return status;
    }

    irp->result = (uintptr_t)addr;
    return OK;
}

static vnode_class_t zeroClass = {
    .name = "const zero",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = const_zero_read,
            [IRP_MJ_MMAP] = const_zero_mmap,
        },
};

static status_t const_null_read(irp_t* irp)
{
    irp->result = 0;
    return OK;
}

static status_t const_null_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    irp->result = sglist_size(frame->write.buffer);
    return OK;
}

static vnode_class_t nullClass = {
    .name = "const null",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = const_null_read,
            [IRP_MJ_WRITE] = const_null_write,
        },
};

static vnode_class_t constClass = {
    .name = "const",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

void const_init(void)
{
    constDir = devfs_dentry_new(NULL, "const", &constClass, NULL);
    if (constDir == NULL)
    {
        panic(NULL, "failed to init const directory\n");
    }

    oneFile = devfs_dentry_new(constDir, "one", &oneClass, NULL);
    if (oneFile == NULL)
    {
        UNREF(constDir);
        panic(NULL, "failed to init one file\n");
    }

    zeroFile = devfs_dentry_new(constDir, "zero", &zeroClass, NULL);
    if (zeroFile == NULL)
    {
        UNREF(constDir);
        UNREF(oneFile);
        panic(NULL, "failed to init zero file\n");
    }

    nullFile = devfs_dentry_new(constDir, "null", &nullClass, NULL);
    if (nullFile == NULL)
    {
        UNREF(constDir);
        UNREF(oneFile);
        UNREF(zeroFile);
        panic(NULL, "failed to init null file\n");
    }
}