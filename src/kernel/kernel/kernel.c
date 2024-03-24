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
#include "scheduler/scheduler.h"
#include "utils/utils.h"
#include "process/process.h"
#include "lock/lock.h"
#include "irq/irq.h"
#include "pic/pic.h"
#include "vfs/vfs.h"
#include "ram_disk/ram_disk.h"
#include "registers/registers.h"
#include "interrupts/interrupts.h"
#include "program_loader/program_loader.h"

static void deallocate_boot_info(BootInfo* bootInfo)
{
    tty_start_message("Deallocating boot info");

    EfiMemoryMap* memoryMap = &bootInfo->memoryMap;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* descriptor = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (descriptor->type == EFI_MEMORY_TYPE_BOOT_INFO)
        {
            pmm_free_pages(descriptor->physicalStart, descriptor->amountOfPages);
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

    scheduler_start();
    program_loader_init();

    vfs_init();
    ram_disk_init(bootInfo->ramRoot);

    deallocate_boot_info(bootInfo);
}

void kernel_cpu_init(void)
{
    Cpu* cpu = smp_self_brute();
    msr_write(MSR_CPU_ID, cpu->id);

    local_apic_init();

    gdt_load();
    gdt_load_tss(&cpu->tss);

    idt_load();

    cr4_write(cr4_read() | CR4_PAGE_GLOBAL_ENABLE);
}