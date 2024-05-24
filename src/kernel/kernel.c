#include "kernel.h"

#include "gdt.h"
#include "idt.h"
#include "tty.h"
#include "heap.h"
#include "pmm.h"
#include "hpet.h"
#include "time.h"
#include "apic.h"
#include "madt.h"
#include "vmm.h"
#include "rsdt.h"
#include "smp.h"
#include "const.h"
#include "sched.h"
#include "utils.h"
#include "process.h"
#include "lock.h"
#include "irq.h"
#include "pic.h"
#include "vfs.h"
#include "sysfs.h"
#include "ramfs.h"
#include "regs.h"
#include "ps2.h"
#include "renderer.h"
#include "loader.h"

static void boot_info_deallocate(BootInfo* bootInfo)
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
    sysfs_init();
    ramfs_init(bootInfo->ramRoot);

    ps2_init();
    const_init();
    renderer_init(&bootInfo->gopBuffer);

    boot_info_deallocate(bootInfo);
}

void kernel_cpu_init(void)
{
    Cpu* cpu = smp_self_brute();
    msr_write(MSR_CPU_ID, cpu->id);

    local_apic_init();

    gdt_load();
    idt_load();
    gdt_load_tss(&cpu->tss);

    cr4_write(cr4_read() | CR4_PAGE_GLOBAL_ENABLE);
}