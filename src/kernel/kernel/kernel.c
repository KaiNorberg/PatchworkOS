#include "kernel.h"

#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "heap/heap.h"
#include "utils/utils.h"
#include "page_allocator/page_allocator.h"
#include "io/io.h"
#include "hpet/hpet.h"
#include "time/time.h"
#include "tss/tss.h"
#include "apic/apic.h"
#include "global_heap/global_heap.h"
#include "madt/madt.h"
#include "ram_disk/ram_disk.h"
#include "device_disk/device_disk.h"

#include "vfs/vfs.h"

#include "master/master.h"
#include "worker_pool/worker_pool.h"

#include <common/common.h>

void kernel_init(BootInfo* bootInfo)
{   
    asm volatile("cli");

    tty_init(&bootInfo->gopBuffer, &bootInfo->font);
    tty_print("Hello from the kernel!\n\r");

    page_allocator_init(&bootInfo->memoryMap);
    page_directory_init(&bootInfo->memoryMap, &bootInfo->gopBuffer);
    heap_init();
    global_heap_init();

    gdt_init();

    rsdt_init(bootInfo->rsdp);
    madt_init();
    apic_init();
    
    hpet_init();
    time_init();

    pid_init();
    
    master_init();
    worker_pool_init();

    vfs_init();
    device_disk_init();
    ram_disk_init(bootInfo->ramRoot);
}