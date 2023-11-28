#include "kernel.h"

#include "kernel/gdt/gdt.h"
#include "kernel/tty/tty.h"
#include "kernel/idt/idt.h"
#include "kernel/utils/utils.h"
#include "kernel/file_system/file_system.h"
#include "kernel/page_allocator/page_allocator.h"
#include "kernel/virtual_memory/virtual_memory.h"

#include "libc/include/stdio.h"

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

VirtualAddressSpace* kernelAddressSpace;

void kernel_init(BootInfo* bootInfo)
{    
    tty_init(bootInfo->Screenbuffer, bootInfo->TTYFont);

    tty_print("Hello from the kernel!\n\r");

    tty_start_message("Page allocator initializing");
    page_allocator_init(bootInfo->MemoryMap, bootInfo->Screenbuffer);
    tty_end_message(TTY_MESSAGE_OK);

    tty_start_message("Initializing kernel address space");
    kernelAddressSpace = virtual_memory_create();
    for (uint64_t i = 0; i < bootInfo->Screenbuffer->Size + 4096; i += 4096)
    {
        virtual_memory_remap(kernelAddressSpace, (void*)((uint64_t)bootInfo->Screenbuffer->Base + i), (void*)((uint64_t)bootInfo->Screenbuffer->Base + i));
    }
    virtual_memory_load_space(kernelAddressSpace);
    tty_end_message(TTY_MESSAGE_OK);

    tty_clear();

    tty_print("Paging and virtual memory has been initialized\n\r");

    tty_start_message("GDT loading");
    static GDTDesc gdtDesc;
	gdtDesc.Size = sizeof(GDT) - 1;
	gdtDesc.Offset = (uint64_t)&gdt;
	gdt_load(&gdtDesc);
    tty_end_message(TTY_MESSAGE_OK);

    tty_start_message("IDT initializing");
    idt_init();
    tty_end_message(TTY_MESSAGE_OK);

    tty_start_message("File system initializing");
    file_system_init(bootInfo->RootDirectory);
    tty_end_message(TTY_MESSAGE_OK);

    tty_print("\n\r");
}
