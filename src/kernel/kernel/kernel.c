#include "kernel.h"

#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "heap/heap.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "ram_disk/ram_disk.h"
#include "page_allocator/page_allocator.h"
#include "io/io.h"
#include "hpet/hpet.h"
#include "interrupts/interrupts.h"
#include "time/time.h"
#include "tss/tss.h"
#include "apic/apic.h"
#include "global_heap/global_heap.h"
#include "madt/madt.h"
#include "workers/workers.h"
#include "master/master.h"

#include "../common.h"

void kernel_init(BootInfo* bootInfo)
{    
    tty_init(bootInfo->framebuffer, bootInfo->font);
    tty_print("Hello from the kernel!\n\r");

    page_allocator_init(bootInfo->memoryMap);
    page_directory_init(bootInfo->memoryMap, bootInfo->framebuffer);
    heap_init();
    global_heap_init();

    idt_init();
    tss_init();
    gdt_init();

    rsdt_init(bootInfo->xsdp);
    madt_init();
    apic_init();

    interrupts_init();

    ram_disk_init(bootInfo->rootDirectory);
    
    hpet_init();

    //scheduler_init();
    //pid_init();
    
    master_init();
    workers_init();

    while (1)
    {
        asm volatile("hlt");
    }

    //kernel_process_init();
    //task_balancer_init();
}