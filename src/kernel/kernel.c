#include "kernel.h"

#include "acpi.h"
#include "apic.h"
#include "const.h"
#include "dwm/dwm.h"
#include "gdt.h"
#include "hpet.h"
#include "idt.h"
#include "log.h"
#include "madt.h"
#include "pic.h"
#include "pmm.h"
#include "ps2/ps2.h"
#include "ramfs.h"
#include "regs.h"
#include "sched.h"
#include "simd.h"
#include "smp.h"
#include "sysfs.h"
#include "time.h"
#include "vfs.h"
#include "vmm.h"

#include <bootloader/boot_info.h>
#include <stdlib_internal/init.h>

#include <stdlib.h>

static void kernel_free_boot_data(efi_mem_map_t* memoryMap)
{
    efi_mem_map_t memoryMapCopy = *memoryMap;
    memoryMap->base = malloc(memoryMap->descriptorSize * memoryMap->descriptorAmount * 2);

    for (uint64_t i = 0; i < memoryMapCopy.descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(&memoryMapCopy, i);

        if (desc->type == EFI_LOADER_DATA)
        {
            pmm_free_pages(desc->physicalStart, desc->amountOfPages);
            log_print("boot data: free [%a-%a]", desc->physicalStart,
                ((uintptr_t)desc->physicalStart) + desc->amountOfPages * PAGE_SIZE);
        }
    }

    free(memoryMap->base);
}

void kernel_init(boot_info_t* bootInfo)
{
    gdt_init();
    idt_init();

    log_init();

    pmm_init(&bootInfo->memoryMap);
    vmm_init(&bootInfo->memoryMap, &bootInfo->kernel, &bootInfo->gopBuffer);

    log_enable_screen(&bootInfo->gopBuffer);

    _StdInit();

    acpi_init(bootInfo->rsdp);
    hpet_init();
    madt_init();
    apic_init();

    time_init();
    log_enable_time();

    pic_init();

    smp_init();
    kernel_cpu_init();

    sched_start();

    vfs_init();
    sysfs_init();
    ramfs_init(bootInfo->ramRoot);

    const_init();
    ps2_init();
    dwm_init(&bootInfo->gopBuffer);

    kernel_free_boot_data(&bootInfo->memoryMap);

    dwm_start();
    log_disable_screen();
}

void kernel_cpu_init(void)
{
    cpu_t* cpu = smp_self_brute();
    msr_write(MSR_CPU_ID, cpu->id);
    gdt_load_tss(&cpu->tss);

    lapic_init();
    simd_init();

    vmm_cpu_init();

    log_print("CPU %d: initialized", (uint64_t)cpu->id);
}
