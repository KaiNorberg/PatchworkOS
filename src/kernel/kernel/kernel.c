#include "kernel.h"

#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "heap/heap.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "file_system/file_system.h"
#include "page_allocator/page_allocator.h"
#include "scheduler/scheduler.h"
#include "io/io.h"
#include "hpet/hpet.h"
#include "interrupts/interrupts.h"
#include "time/time.h"
#include "tss/tss.h"
#include "apic/apic.h"
#include "smp/smp.h"
#include "global_heap/global_heap.h"

#include "../common.h"

void kernel_core_entry()
{  
    tty_print("Hello from other core!\n\r");
    while(1)
    {
        asm volatile("hlt");
    }
}

void kernel_init(BootInfo* bootInfo)
{    
    tty_init(bootInfo->framebuffer, bootInfo->font);
    tty_print("Hello from the kernel!\n\r");

    page_allocator_init(bootInfo->memoryMap, bootInfo->framebuffer);
    page_directory_init(bootInfo->memoryMap, bootInfo->framebuffer);
    global_heap_init(bootInfo->memoryMap);

    rsdt_init(bootInfo->xsdp);

    apic_init();

    heap_init();

    tss_init();
    gdt_init();
    idt_init(); 

    interrupts_init();

    file_system_init(bootInfo->rootDirectory);
    
    hpet_init(TICKS_PER_SECOND);
    time_init();

    scheduler_init();
    
    //Disable pic, temporary code
    /*io_outb(PIC1_DATA, 0xFF);
    io_outb(PIC2_DATA, 0xFF);

    smp_init(kernel_core_entry);*/
}