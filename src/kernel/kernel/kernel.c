#include "kernel.h"

#include "gdt/gdt.h"
#include "idt/idt.h"
#include "tty/tty.h"
#include "heap/heap.h"
#include "pmm/pmm.h"
#include "hpet/hpet.h"
#include "time/time.h"
#include "apic/apic.h"
#include "madt/madt.h"
#include "vmm/vmm.h"
#include "rsdt/rsdt.h"
#include "smp/smp.h"
#include "sched/sched.h"
#include "utils/utils.h"
#include "process/process.h"
#include "lock/lock.h"
#include "irq/irq.h"
#include "pic/pic.h"
#include "vfs/vfs.h"
#include "ram_disk/ram_disk.h"
#include "regs/regs.h"
#include "loader/loader.h"

static void deallocate_boot_info(BootInfo* bootInfo)
{
    tty_start_message("Deallocating boot info");

    EfiMemoryMap* memoryMap = &bootInfo->memoryMap;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (desc->type == EFI_MEMORY_TYPE_BOOT_INFO)
        {
            pmm_free_pages(desc->physicalStart, desc->amountOfPages);
        }
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void kernel_init(BootInfo* bootInfo)
{
    pmm_init(&bootInfo->memoryMap);
    vmm_init(&bootInfo->memoryMap);
    heap_init();

    tty_init(&bootInfo->gopBuffer, &bootInfo->font);
    tty_print("Hello from the kernel!\n");

    gdt_init();
    idt_init();

    rsdt_init(bootInfo->rsdp);
    hpet_init();
    madt_init();
    apic_init();

    smp_init();
    kernel_cpu_init();

    pic_init();
    time_init();

    sched_start();

    vfs_init();
    ram_disk_init(bootInfo->ramRoot);

    deallocate_boot_info(bootInfo);
}

void kernel_cpu_init(void)
{
    Cpu* cpu = smp_self_brute();
    MSR_WRITE(MSR_CPU_ID, cpu->id);

    local_apic_init();

    gdt_load();
    idt_load();
    gdt_load_tss(&cpu->tss);

    CR4_WRITE(CR4_READ() | CR4_PAGE_GLOBAL_ENABLE);
}