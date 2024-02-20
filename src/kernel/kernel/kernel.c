#include "kernel.h"

#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "heap/heap.h"
#include "pmm/pmm.h"
#include "hpet/hpet.h"
#include "time/time.h"
#include "tss/tss.h"
#include "apic/apic.h"
#include "madt/madt.h"
#include "ram_disk/ram_disk.h"
#include "device_disk/device_disk.h"
#include "vfs/vfs.h"
#include "vmm/vmm.h"

#include "master/master.h"
#include "worker_pool/worker_pool.h"

#include <libc/string.h>
#include <common/common.h>

static void deallocate_boot_info(BootInfo* bootInfo)
{
    EfiMemoryMap* memoryMap = &bootInfo->memoryMap;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* descriptor = EFI_GET_DESCRIPTOR(memoryMap, i);

        if (descriptor->type == EFI_MEMORY_TYPE_BOOT_INFO)
        {
            pmm_unlock_pages(descriptor->physicalStart, descriptor->amountOfPages);
            
            //For testing
            memset(descriptor->physicalStart, 0, descriptor->amountOfPages * 0x1000);
        }
    }
}

void kernel_init(BootInfo* bootInfo)
{   
    asm volatile("cli");

    pmm_init(&bootInfo->memoryMap);
    vmm_init(&bootInfo->memoryMap);

    pmm_move_to_higher_half();
    bootInfo = vmm_physical_to_virtual(bootInfo);

    tty_init(&bootInfo->gopBuffer, &bootInfo->font);    
    tty_print("Hello from the kernel!\n");

    while (1)
    {
        asm volatile("hlt");
    }
    
    heap_init();

    gdt_init();
    
    rsdt_init(bootInfo->rsdp);
    madt_init();
    apic_init();
        
    hpet_init();
    time_init();

    deallocate_boot_info(bootInfo);

    while (1)
    {
        asm volatile("hlt");
    }

    pid_init();
    
    master_init();
    worker_pool_init();

    vfs_init();
    device_disk_init();
    ram_disk_init(bootInfo->ramRoot);
}