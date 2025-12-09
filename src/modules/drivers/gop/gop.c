#include <kernel/drivers/abstract/fb.h>
#include <kernel/init/init.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/sched/process.h>
#include <kernel/sched/sched.h>

#include <errno.h>
#include <string.h>
#include <sys/fb.h>
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

static void* gop_mmap(fb_t* fb, void* addr, uint64_t length, uint64_t* offset, pml_flags_t flags)
{
    (void)fb; // Unused

    process_t* process = sched_process();

    uintptr_t physAddr = (uint64_t)gop.physAddr + *offset;
    uintptr_t endAddr = physAddr + length;
    if (endAddr > (uint64_t)gop.physAddr + (gop.stride * gop.height * sizeof(uint32_t)))
    {
        errno = EINVAL;
        return NULL;
    }

    addr = vmm_map(&process->space, addr, (void*)physAddr, length, flags, NULL, NULL);
    if (addr == NULL)
    {
        return NULL;
    }
    return addr;
}

static fb_info_t info;

static uint64_t gop_init(void)
{
    boot_info_t* bootInfo = init_boot_info_get();
    if (bootInfo == NULL || bootInfo->gop.virtAddr == NULL)
    {
        LOG_ERR("no GOP provided by bootloader");
        return ERR;
    }

    gop = bootInfo->gop;
    info.width = bootInfo->gop.width;
    info.height = bootInfo->gop.height;
    info.stride = bootInfo->gop.stride;
    info.format = FB_ARGB32;
    strncpy(info.name, "GOP Framebuffer", MAX_NAME - 1);
    info.name[MAX_NAME - 1] = '\0';

    fb = fb_new(&info, gop_mmap);
    if (fb == NULL)
    {
        LOG_ERR("Failed to create GOP framebuffer");
        return ERR;
    }

    LOG_INFO("GOP framebuffer initialized %ux%u\n", info.width, info.height);
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