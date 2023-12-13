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

void kernel_init(BootInfo* bootInfo)
{    
    tty_init(bootInfo->Screenbuffer, bootInfo->TTYFont);
    tty_print("Hello from the kernel!\n\r");

    page_allocator_init(bootInfo->MemoryMap, bootInfo->Screenbuffer);

    virtual_memory_init(bootInfo->MemoryMap, bootInfo->Screenbuffer);

    //Task State Segment Values
    void* RSP0 = page_allocator_request() + 0x1000;
    void* RSP1 = page_allocator_request() + 0x1000;
    void* RSP2 = page_allocator_request() + 0x1000;

    gdt_init(RSP0, RSP1, RSP2);

    acpi_init(bootInfo->Xsdp);

    idt_init();
    
    heap_init();
    
    file_system_init(bootInfo->RootDirectory);
    
    syscall_init();

    multitasking_init();
}
