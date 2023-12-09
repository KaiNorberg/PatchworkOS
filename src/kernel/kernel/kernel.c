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

__attribute__((aligned(0x1000)))
GDT gdt = 
{
    {0, 0, 0, 0x00, 0x00, 0}, //NULL
    {0, 0, 0, 0x9A, 0xA0, 0}, //KernelCode
    {0, 0, 0, 0x92, 0xA0, 0}, //KernelData
    {0, 0, 0, 0x00, 0x00, 0}, //UserNull
    {0, 0, 0, 0x9A, 0xA0, 0}, //UserCode
    {0, 0, 0, 0x92, 0xA0, 0}, //UserData
};

void kernel_init(BootInfo* bootInfo)
{    
    tty_init(bootInfo->Screenbuffer, bootInfo->TTYFont);
    tty_print("Hello from the kernel!\n\r");

    page_allocator_init(bootInfo->MemoryMap, bootInfo->Screenbuffer);

    virtual_memory_init(bootInfo->MemoryMap);

    tty_clear();
    tty_print("Paging and virtual memory have been initialized\n\r");

    tty_start_message("GDT loading");
    static GDTDesc gdtDesc;
	gdtDesc.Size = sizeof(GDT) - 1;
	gdtDesc.Offset = (uint64_t)&gdt;
	gdt_load(&gdtDesc);
    tty_end_message(TTY_MESSAGE_OK);

    idt_init();
    
    heap_init();
    
    file_system_init(bootInfo->RootDirectory);
    
    syscall_init(kernelAddressSpace);

    multitasking_init();
}
