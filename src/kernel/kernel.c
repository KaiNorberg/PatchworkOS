#include "kernel.h"

#include "acpi/acpi.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/simd.h"
#include "cpu/smp.h"
#include "cpu/syscalls.h"
#include "drivers/apic.h"
#include "drivers/const.h"
#include "drivers/fb/gop.h"
#include "drivers/ps2/ps2.h"
#include "drivers/time/rtc.h"
#include "fs/ramfs.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "gnu-efi/inc/efidef.h"
#include "ipc/pipe.h"
#include "ipc/shmem.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "net/net.h"
#include "proc/process.h"
#include "sched/sched.h"
#include "sched/timer.h"
#include "sched/wait.h"

#include <boot/boot_info.h>
#include <libstd/_internal/init.h>
#include <strings.h>

static void kernel_free_loader_data(boot_memory_map_t* map)
{
    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (desc->Type == EfiLoaderData)
        {
            pmm_free_pages((void*)desc->VirtualStart, desc->NumberOfPages);
            LOG_INFO("free boot memory [0x%016lx-0x%016lx]\n", desc->VirtualStart,
                ((uintptr_t)desc->VirtualStart) + desc->NumberOfPages * PAGE_SIZE);
        }
    }

    LOG_INFO("kernel initalized using %llu kb of memory\n", pmm_reserved_amount() * PAGE_SIZE / 1024);
}

void kernel_init(boot_info_t* bootInfo)
{
    gdt_cpu_init();
    idt_cpu_init();

    smp_bootstrap_init();
    gdt_cpu_load_tss(&smp_self_unsafe()->tss);

    log_init(&bootInfo->gop);

    pmm_init(&bootInfo->memory.map);
    vmm_init(&bootInfo->memory, &bootInfo->gop, &bootInfo->kernel);
    heap_init();

    panic_symbols_init(&bootInfo->kernel);

    _std_init();

    process_kernel_init();
    sched_init();
    wait_init();

    vfs_init();
    ramfs_init(&bootInfo->disk);
    sysfs_init();

    acpi_init(bootInfo->rsdp, &bootInfo->memory.map);

    lapic_init();
    lapic_cpu_init();
    ioapic_all_init();

    rtc_init();

    timer_init();
    timer_cpu_init();

    log_file_expose();
    process_procfs_init();

    simd_cpu_init();

    syscall_table_init();
    syscalls_cpu_init();

    const_init();
    ps2_init();
    net_init();
    pipe_init();
    shmem_init();
    gop_init(&bootInfo->gop);
    statistics_init();

    smp_others_init();

    kernel_free_loader_data(&bootInfo->memory.map);
    vmm_unmap_lower_half();
}

void kernel_other_init(void)
{
    gdt_cpu_init();
    idt_cpu_init();

    gdt_cpu_load_tss(&smp_self_unsafe()->tss);

    lapic_cpu_init();
    simd_cpu_init();

    vmm_cpu_init();
    syscalls_cpu_init();
    timer_cpu_init();
}
