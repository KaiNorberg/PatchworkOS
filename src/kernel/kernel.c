#include "kernel.h"

#include "acpi.h"
#include "apic.h"
#include "const.h"
#include "fb.h"
#include "gdt.h"
#include "gop.h"
#include "hpet.h"
#include "idt.h"
#include "log.h"
#include "madt.h"
#include "net/net.h"
#include "pic.h"
#include "pipe.h"
#include "pmm.h"
#include "ps2/ps2.h"
#include "ramfs.h"
#include "regs.h"
#include "sched.h"
#include "simd.h"
#include "smp.h"
#include "sysfs.h"
#include "systime.h"
#include "testing.h"
#include "vfs.h"
#include "vmm.h"
#include "waitsys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>
#include <bootloader/boot_info.h>
#include <libstd_internal/init.h>

static void kernel_free_loader_data(efi_mem_map_t* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (desc->type == EFI_LOADER_DATA)
        {
            pmm_free_pages(VMM_LOWER_TO_HIGHER(desc->physicalStart), desc->amountOfPages);
            printf("loader data: free [0x%016lx-0x%016lx]", desc->physicalStart,
                ((uintptr_t)desc->physicalStart) + desc->amountOfPages * PAGE_SIZE);
        }
    }
}

void kernel_init(boot_info_t* bootInfo)
{
    gdt_init();
    idt_init();
    smp_init();

    log_init();

    pmm_init(&bootInfo->memoryMap);
    vmm_init(&bootInfo->memoryMap, &bootInfo->kernel, &bootInfo->gopBuffer);

    log_enable_screen(&bootInfo->gopBuffer);

    _StdInit();

    sysfs_init();
    vfs_init();
    sysfs_mount_to_vfs();
    ramfs_init(&bootInfo->ramDisk);

    log_expose();
    process_backend_init();

    sched_init();
    waitsys_init();

    acpi_init(bootInfo->rsdp);
    hpet_init();
    madt_init();
    apic_init();
    lapic_init();

    pic_init();
    simd_init();
    systime_init();
    log_enable_time();

    smp_init_others();
    systime_timer_init();

    const_init();
    ps2_init();
    net_init();
    pipe_init();
    gop_init(&bootInfo->gopBuffer);

    kernel_free_loader_data(&bootInfo->memoryMap);

#ifdef TESTING
    testing_run_tests();
#endif

    asm volatile("sti");
}
