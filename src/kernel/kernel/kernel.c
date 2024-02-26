#include "kernel.h"

#include <stdint.h>

#include "gdt/gdt.h"
#include "tty/tty.h"
#include "heap/heap.h"
#include "pmm/pmm.h"
#include "hpet/hpet.h"
#include "time/time.h"
#include "apic/apic.h"
#include "madt/madt.h"
#include "ram_disk/ram_disk.h"
#include "device_disk/device_disk.h"
#include "vfs/vfs.h"
#include "vmm/vmm.h"
#include "master/master.h"
#include "worker_pool/worker_pool.h"
#include <common/boot_info/boot_info.h>
#include "rsdt/rsdt.h"
#include "worker/process/process.h"

static void deallocate_boot_info(BootInfo* bootInfo)
{   
    tty_start_message("Deallocating boot info");

    EfiMemoryMap* memoryMap = &bootInfo->memoryMap;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* descriptor = vmm_physical_to_virtual(EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i));

        if (descriptor->type == EFI_MEMORY_TYPE_BOOT_INFO)
        {
            pmm_free_pages(descriptor->physicalStart, descriptor->amountOfPages);
        }
    }

    tty_end_message(TTY_MESSAGE_OK);
}

char buffer[32];

void kernel_init(BootInfo* bootInfo)
{   
    asm volatile("cli");

    pmm_init(&bootInfo->memoryMap);
    vmm_init(&bootInfo->memoryMap);

    tty_init(&bootInfo->gopBuffer, &bootInfo->font);    
    tty_print("Hello from the kernel!\n");

    heap_init();
    gdt_init();
    
    rsdt_init(bootInfo->rsdp);
    while (1)
    {
        asm volatile("hlt");
    }
    hpet_init();
    madt_init();
    apic_init();

    time_init();
    pid_init();

    master_init();
    worker_pool_init();

    /*vfs_init();
    //device_disk_init();
    ram_disk_init(bootInfo->ramRoot);

    File* file = 0;
    Status status = vfs_open(&file, "ram:/test/test.txt", FILE_FLAG_READ);
    tty_print("OPEN: ");
    tty_print(statusToString[status]);
    tty_print("\n");

    status = vfs_read(file, buffer, 32);
    tty_print("READ: ");
    tty_print(statusToString[status]);
    tty_print("\n");

    tty_print(buffer);
    tty_print("\n");

    status = vfs_close(file);
    tty_print("CLOSE: ");
    tty_print(statusToString[status]);
    tty_print("\n");*/

    deallocate_boot_info(bootInfo);
    
    while (1)
    {
        asm volatile("hlt");
    }
}