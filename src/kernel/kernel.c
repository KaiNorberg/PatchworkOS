#include "kernel.h"

#include "acpi/acpi.h"
#include "acpi/madt.h"
#include "cpu/apic.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/pic.h"
#include "cpu/simd.h"
#include "cpu/smp.h"
#include "drivers/const.h"
#include "drivers/fb/gop.h"
#include "drivers/ps2/ps2.h"
#include "drivers/systime/hpet.h"
#include "drivers/systime/systime.h"
#include "fs/ramfs.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "ipc/pipe.h"
#include "ipc/shmem.h"
#include "log/log.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "net/net.h"
#include "proc/process.h"
#include "sched/sched.h"
#include "sched/wait.h"

#ifndef NDEBUG
#include "utils/testing.h"
#endif

#include <boot/boot_info.h>
#include <libstd/_internal/init.h>
#include <strings.h>

static void kernel_free_loader_data(efi_mem_map_t* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (desc->type == EFI_LOADER_DATA)
        {
            pmm_free_pages(PML_LOWER_TO_HIGHER(desc->physicalStart), desc->amountOfPages);
            LOG_INFO("loader data: free [0x%016lx-0x%016lx]\n", desc->physicalStart,
                ((uintptr_t)desc->physicalStart) + desc->amountOfPages * PAGE_SIZE);
        }
    }
}

void kernel_init(boot_info_t* bootInfo)
{
    gdt_cpu_init();
    idt_cpu_init();

    smp_bootstrap_init();
    gd_cpu_load_tss(&smp_self_unsafe()->tss);

    log_init();

    pmm_init(&bootInfo->memoryMap);
    vmm_init(&bootInfo->memoryMap, &bootInfo->kernel, &bootInfo->gopBuffer);
    heap_init();

    log_screen_enable(&bootInfo->gopBuffer);

    _std_init();

    acpi_init(bootInfo->rsdp);
    hpet_init();
    systime_init();
    log_enable_time();

    vfs_init();
    ramfs_init(&bootInfo->ramDisk);
    sysfs_init();

    log_file_expose();
    process_backend_init();

    sched_init();

    madt_init();
    apic_init();
    lapic_cpu_init();

    pic_init();
    simd_cpu_init();

    smp_others_init();
    systime_cpu_timer_init();

    syscall_table_init();
    syscalls_cpu_init();

    const_init();
    ps2_init();
    net_init();
    pipe_init();
    shmem_init();
    gop_init(&bootInfo->gopBuffer);
    statistics_init();

    kernel_free_loader_data(&bootInfo->memoryMap);

#ifndef NDEBUG
    testing_run_tests();
#endif
}

void kernel_other_init(void)
{
    gdt_cpu_init();
    idt_cpu_init();

    gd_cpu_load_tss(&smp_self_brute()->tss);

    lapic_cpu_init();
    simd_cpu_init();

    vmm_cpu_init();
    syscalls_cpu_init();
    systime_cpu_timer_init();
}
