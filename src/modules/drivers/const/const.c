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
#include <sys/ioring.h>
#include <sys/status.h>

/**
 * @brief Constant devices
 * @defgroup kernel_drivers_const Constant Devices
 * @ingroup kernel_drivers
 *
 * This module provides the constant devices which provide user space with its primary means of allocating memory and
 * obtaining constant data.
 *
 * The constant devices are exposed under the `/dev/const/` directory:
 * - `/dev/const/one`: A readable and mappable file that returns bytes with all bits set to 1.
 * - `/dev/const/zero`: A readable and mappable file that returns bytes with all bits set to 0.
 * - `/dev/const/null`: A readable and writable file that discards all written data and returns EOF on read.
 *
 * @{
 */

static dentry_t* constDir;
static dentry_t* oneFile;
static dentry_t* zeroFile;
static dentry_t* nullFile;

static status_t const_one_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    return mdl_fill(frame->read.buffer, SIZE_MAX, 0, &irp->result, UINT8_MAX);
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
    .type = VNODE_REGULAR,
    .handlers =
        {
            [IRP_MJ_READ] = const_one_read,
            [IRP_MJ_MMAP] = const_one_mmap,
        },
};

static status_t const_zero_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    return mdl_fill(frame->read.buffer, SIZE_MAX, 0, &irp->result, 0);
}

static status_t const_zero_mmap(irp_t* irp)
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

    memset(addr, 0, length);
    irp->result = (uintptr_t)addr;
    return OK;
}

static vnode_class_t zeroClass = {
    .name = "const zero",
    .type = VNODE_REGULAR,
    .handlers =
        {
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
    irp->result = mdl_size(frame->write.buffer);
    return OK;
}

static vnode_class_t nullClass = {
    .name = "const null",
    .type = VNODE_REGULAR,
    .handlers =
        {
            [IRP_MJ_READ] = const_null_read,
            [IRP_MJ_WRITE] = const_null_write,
        },
};

static vnode_class_t constClass = {
    .name = "const",
    .type = VNODE_DIR,
    .iterate = dentry_generic_iterate,
};

static status_t const_init(void)
{
    constDir = devfs_dentry_new(NULL, "const", &constClass, NULL);
    if (constDir == NULL)
    {
        LOG_ERR("failed to init const directory\n");
        return ERR(DRIVER, NOMEM);
    }

    oneFile = devfs_dentry_new(constDir, "one", &oneClass, NULL);
    if (oneFile == NULL)
    {
        UNREF(constDir);
        LOG_ERR("failed to init one file\n");
        return ERR(DRIVER, NOMEM);
    }

    zeroFile = devfs_dentry_new(constDir, "zero", &zeroClass, NULL);
    if (zeroFile == NULL)
    {
        UNREF(constDir);
        UNREF(oneFile);
        LOG_ERR("failed to init zero file\n");
        return ERR(DRIVER, NOMEM);
    }

    nullFile = devfs_dentry_new(constDir, "null", &nullClass, NULL);
    if (nullFile == NULL)
    {
        UNREF(constDir);
        UNREF(oneFile);
        UNREF(zeroFile);
        LOG_ERR("failed to init null file\n");
        return ERR(DRIVER, NOMEM);
    }

    return OK;
}

static void const_deinit(void)
{
    UNREF(constDir);
    UNREF(oneFile);
    UNREF(zeroFile);
    UNREF(nullFile);
}

/** @} */

status_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        return const_init();
    case MODULE_EVENT_UNLOAD:
        const_deinit();
        break;
    default:
        break;
    }

    return OK;
}

MODULE_INFO("Const Driver", "Kai Norberg", "A constant device driver", OS_VERSION, "MIT", "BOOT_ALWAYS");