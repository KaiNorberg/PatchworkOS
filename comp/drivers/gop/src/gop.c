#include <kernel/drivers/abstract/fb.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/log/screen.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/start/boot_info.h>
#include <kernel/start/start.h>

#include <string.h>
#include <sys/math.h>

/**
 * @brief GOP (Graphics Output Protocol) driver.
 * @defgroup kernel_drivers_gop GOP Driver
 * @ingroup kernel_drivers
 *
 * This module provides a framebuffer device for the GOP framebuffer provided by the bootloader.
 *
 * @{
 */

static boot_gop_t gop;
static fb_t fb;

static status_t gop_read(irp_t* irp)
{
    screen_hide();

    size_t fbSize = gop.height * gop.stride * sizeof(uint32_t);
    return irp_read_helper(irp, gop.virtAddr, fbSize);
}

static status_t gop_write(irp_t* irp)
{
    screen_hide();

    size_t fbSize = gop.height * gop.stride * sizeof(uint32_t);
    return irp_write_helper(irp, gop.virtAddr, fbSize);
}

static status_t gop_mmap(irp_t* irp)
{
    screen_hide();

    irp_frame_t* frame = irp_current(irp);

    process_t* process = process_current();

    uintptr_t physAddr = (uintptr_t)gop.physAddr + frame->mmap.offset;
    phys_addr_t endAddr = physAddr + frame->mmap.length;
    if (endAddr > (uintptr_t)gop.physAddr + (gop.stride * gop.height * sizeof(uint32_t)))
    {
        return ERR(DRIVER, INVAL);
    }

    void* addr = frame->mmap.address;
    status_t status = vmm_map(&process->space, &addr, physAddr, frame->mmap.length, frame->mmap.flags, NULL, NULL);
    if (IS_ERR(status))
    {
        return status;
    }

    irp->result = (uintptr_t)addr;
    return OK;
}

static vnode_class_t gopDataClass = {
    .name = "gop data",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = gop_read,
            [IRP_MJ_WRITE] = gop_write,
            [IRP_MJ_MMAP] = gop_mmap,
        },
};

static status_t gop_init(void)
{
    boot_info_t* bootInfo = boot_info_get();
    if (bootInfo == NULL || bootInfo->gop.virtAddr == NULL)
    {
        LOG_ERR("no GOP provided by bootloader");
        return ERR(DRIVER, NOENT);
    }

    gop = bootInfo->gop;

    fb.name = "Graphics Output Protocol";
    fb.width = gop.width;
    fb.height = gop.height;
    fb.pitch = gop.stride * sizeof(uint32_t);
    fb.format = "B8G8R8A8";
    fb.data = &gopDataClass;

    status_t status = fb_register(&fb);
    if (IS_ERR(status))
    {
        LOG_ERR("failed to create GOP framebuffer");
        return status;
    }

    return OK;
}

/** @} */

status_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        return gop_init();
    case MODULE_EVENT_UNLOAD:
        fb_unregister(&fb);
        break;
    default:
        break;
    }
    return OK;
}