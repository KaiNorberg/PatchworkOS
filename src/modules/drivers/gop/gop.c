#include <kernel/drivers/abstract/fb.h>
#include <kernel/init/boot_info.h>
#include <kernel/init/init.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/fs/vfs.h>

#include <errno.h>
#include <string.h>
#include <sys/math.h>

/**
 * @brief GOP (Graphics Output Protocol) driver.
 * @defgroup modules_drivers_gop GOP Driver
 * @ingroup modules_drivers
 *
 * This module provides a framebuffer device for the GOP framebuffer provided by the bootloader.
 *
 * @{
 */

static boot_gop_t gop;
static fb_t* fb;

static uint64_t gop_info(fb_t* fb, fb_info_t* info)
{
    UNUSED(fb);

    info->width = gop.width;
    info->height = gop.height;
    info->pitch = gop.stride * sizeof(uint32_t);
    strncpy(info->format, "B8G8R8A8", FB_MAX_FORMAT);
    return 0;
}

static uint64_t gop_read(fb_t* fb, void* buffer, uint64_t count, uint64_t* offset)
{
    UNUSED(fb);

    log_screen_disable();

    uint64_t fbSize = gop.height * gop.stride * sizeof(uint32_t);
    return BUFFER_READ(buffer, count, offset, ((uint8_t*)gop.virtAddr), fbSize);
}

static uint64_t gop_write(fb_t* fb, const void* buffer, uint64_t count, uint64_t* offset)
{
    UNUSED(fb);

    log_screen_disable();

    uint64_t fbSize = gop.height * gop.stride * sizeof(uint32_t);
    return BUFFER_WRITE(buffer, count, offset, ((uint8_t*)gop.virtAddr), fbSize);
}

static void* gop_mmap(fb_t* fb, void* addr, uint64_t length, uint64_t* offset, pml_flags_t flags)
{
    UNUSED(fb);

    log_screen_disable();

    process_t* process = sched_process();

    uintptr_t physAddr = (uint64_t)gop.physAddr + *offset;
    uintptr_t endAddr = physAddr + length;
    if (endAddr > (uint64_t)gop.physAddr + (gop.stride * gop.height * sizeof(uint32_t)))
    {
        errno = EINVAL;
        return NULL;
    }

    return vmm_map(&process->space, addr, (void*)physAddr, length, flags, NULL, NULL);
}

static fb_ops_t ops = {
    .info = gop_info,
    .read = gop_read,
    .write = gop_write,
    .mmap = gop_mmap,
};

static uint64_t gop_init(void)
{
    boot_info_t* bootInfo = boot_info_get();
    if (bootInfo == NULL || bootInfo->gop.virtAddr == NULL)
    {
        LOG_ERR("no GOP provided by bootloader");
        return ERR;
    }

    gop = bootInfo->gop;
    fb = fb_new("Graphics Output Protocol", &ops, NULL);
    if (fb == NULL)
    {
        LOG_ERR("failed to create GOP framebuffer");
        return ERR;
    }

    return 0;
}

/** @} */

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        if (gop_init() == ERR)
        {
            return ERR;
        }
        break;
    default:
        break;
    }
    return 0;
}

MODULE_INFO("GOP Driver", "Kai Norberg", "A driver for the GOP framebuffer", OS_VERSION, "MIT", "BOOT_GOP");