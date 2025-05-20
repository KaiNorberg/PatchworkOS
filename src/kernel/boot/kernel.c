#include "kernel.h"

#include "acpi/acpi.h"
#include "acpi/madt.h"
#include "cpu/apic.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/pic.h"
#include "cpu/regs.h"
#include "cpu/simd.h"
#include "cpu/smp.h"
#include "drivers/const.h"
#include "drivers/fb/fb.h"
#include "drivers/fb/gop.h"
#include "drivers/ps2/ps2.h"
#include "drivers/systime/hpet.h"
#include "drivers/systime/systime.h"
#include "fs/ramfs.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "ipc/pipe.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "net/net.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "utils/log.h"
#include "utils/testing.h"

#include <bootloader/boot_info.h>
#include <libstd_internal/init.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/argsplit.h>

static void kernel_free_loader_data(efi_mem_map_t* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (desc->type == EFI_LOADER_DATA)
        {
            pmm_free_pages(VMM_LOWER_TO_HIGHER(desc->physicalStart), desc->amountOfPages);
            printf("loader data: free [0x%016lx-0x%016lx]\n", desc->physicalStart,
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

    syscall_init();

    const_init();
    ps2_init();
    net_init();
    pipe_init();
    gop_init(&bootInfo->gopBuffer);
    statistics_init();

    kernel_free_loader_data(&bootInfo->memoryMap);

#ifdef TESTING
    testing_run_tests();
#endif

    asm volatile("sti");
}
