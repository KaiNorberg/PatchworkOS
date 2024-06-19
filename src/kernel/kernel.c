#include "kernel.h"

#include "apic.h"
#include "common/boot_info.h"
#include "const.h"
#include "debug.h"
#include "dwm.h"
#include "gdt.h"
#include "hpet.h"
#include "idt.h"
#include "madt.h"
#include "pic.h"
#include "pmm.h"
#include "ps2.h"
#include "ramfs.h"
#include "regs.h"
#include "rsdt.h"
#include "sched.h"
#include "simd.h"
#include "smp.h"
#include "splash.h"
#include "stdlib.h"
#include "sysfs.h"
#include "time.h"
#include "vfs.h"
#include "vmm.h"

#include <libs/std/internal/init.h>

static void boot_info_deallocate(boot_info_t* bootInfo)
{
    efi_mem_map_t* memoryMap = &bootInfo->memoryMap;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (desc->type == EFI_BOOT_INFO)
        {
            pmm_free_pages(desc->physicalStart, desc->amountOfPages);
        }
    }
}

void kernel_init(boot_info_t* bootInfo)
{
    pmm_init(&bootInfo->memoryMap);
    vmm_init(&bootInfo->memoryMap, &bootInfo->gopBuffer);

    _StdInit();

    splash_init(&bootInfo->gopBuffer, &bootInfo->font);
    debug_init(&bootInfo->gopBuffer, &bootInfo->font);

    gdt_init();
    idt_init();

    rsdt_init(bootInfo->rsdp);
    hpet_init();
    madt_init();
    apic_init();

    pic_init();
    time_init();

    smp_init();
    kernel_cpu_init();

    sched_start();

    vfs_init();
    sysfs_init();
    ramfs_init(bootInfo->ramRoot);

    ps2_init();
    const_init();
    dwm_init(&bootInfo->gopBuffer);

    boot_info_deallocate(bootInfo);
    splash_cleanup();

    dwm_start();
}

void kernel_cpu_init(void)
{
    cpu_t* cpu = smp_self_brute();
    msr_write(MSR_CPU_ID, cpu->id);

    lapic_init();
    simd_init();

    gdt_load();
    idt_load();
    gdt_load_tss(&cpu->tss);

    cr4_write(cr4_read() | CR4_PAGE_GLOBAL_ENABLE);
}
