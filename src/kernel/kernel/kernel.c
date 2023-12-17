#include "kernel.h"

#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "heap/heap.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "file_system/file_system.h"
#include "page_allocator/page_allocator.h"
#include "multitasking/multitasking.h"
#include "acpi/acpi.h"
#include "io/io.h"
#include "rtc/rtc.h"

void kernel_init(BootInfo* bootInfo)
{    
    tty_init(bootInfo->framebuffer, bootInfo->font);
    tty_print("Hello from the kernel!\n\r");

    page_allocator_init(bootInfo->memoryMap, bootInfo->framebuffer);

    virtual_memory_init(bootInfo->memoryMap);

    void* RSP0 = page_allocator_request() + 0x1000;
    gdt_init(RSP0, RSP0, RSP0);

    acpi_init(bootInfo->xsdp);

    idt_init();
    
    heap_init();
    
    file_system_init(bootInfo->rootDirectory);
    
    syscall_init();

    multitasking_init();

    rtc_init(6); // 1024 HZ

    io_pic_clear_mask(IRQ_KEYBOARD);
}
