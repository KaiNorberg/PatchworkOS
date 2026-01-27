#include <kernel/drivers/abstract/fb.h>
#include <kernel/fs/vfs.h>
#include <kernel/init/boot_info.h>
#include <kernel/init/init.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/log/screen.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>

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

static status_t gop_info(fb_t* fb, fb_info_t* info)
{
    UNUSED(fb);

    info->width = gop.width;
    info->height = gop.height;
    info->pitch = gop.stride * sizeof(uint32_t);
    strncpy(info->format, "B8G8R8A8", sizeof(info->format));
    return OK;
}

static status_t gop_read(fb_t* fb, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    UNUSED(fb);

    screen_hide();

    size_t fbSize = gop.height * gop.stride * sizeof(uint32_t);
    return buffer_read(buffer, count, offset, bytesRead, (void*)gop.virtAddr, fbSize);
}

static status_t gop_write(fb_t* fb, const void* buffer, size_t count, size_t* offset, size_t* bytesWritten)
{
    UNUSED(fb);

    screen_hide();

    size_t fbSize = gop.height * gop.stride * sizeof(uint32_t);
    *bytesWritten = BUFFER_WRITE(buffer, count, offset, ((uint8_t*)gop.virtAddr), fbSize);
    return OK;
}

static status_t gop_mmap(fb_t* fb, void** addr, size_t length, size_t* offset, pml_flags_t flags)
{
    UNUSED(fb);

    screen_hide();

    process_t* process = process_current();

    uintptr_t physAddr = (uintptr_t)gop.physAddr + *offset;
    phys_addr_t endAddr = physAddr + length;
    if (endAddr > (uintptr_t)gop.physAddr + (gop.stride * gop.height * sizeof(uint32_t)))
    {
        return ERR(DRIVER, INVAL);
    }

    return vmm_map(&process->space, addr, physAddr, length, flags, NULL, NULL);
}

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
    fb.info = gop_info;
    fb.read = gop_read;
    fb.write = gop_write;
    fb.mmap = gop_mmap;

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

MODULE_INFO("GOP Driver", "Kai Norberg", "A driver for the GOP framebuffer", OS_VERSION, "MIT", "BOOT_GOP");