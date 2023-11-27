#include "entry.h"

#include "kernel/gdt/gdt.h"
#include "kernel/tty/tty.h"
#include "kernel/idt/idt.h"
#include "kernel/utils/utils.h"
#include "kernel/file_system/file_system.h"
#include "kernel/page_allocator/page_allocator.h"

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

void _start(BootInfo* bootInfo)
{
    static GDTDesc gdtDesc;
	gdtDesc.Size = sizeof(GDT) - 1;
	gdtDesc.Offset = (uint64_t)&gdt;
	gdt_load(&gdtDesc);

    page_allocator_init(bootInfo->MemoryMap, bootInfo->Screenbuffer);

    idt_init();

    tty_init(bootInfo->Screenbuffer, bootInfo->PSFFonts[0]);

    file_system_init(bootInfo->RootDirectory);

    tty_print("Total Page Amount");
    tty_printi(page_allocator_get_total_amount());
    tty_print("Total Memory Size (MB)");
    tty_printi((page_allocator_get_total_amount() * 4096) / 1048576);
    tty_print("Total Memory Size (GB)");
    tty_printi((page_allocator_get_total_amount() * 4096) / 1073741824);

    tty_put('\n');

    tty_print("Locked Page Amount");
    tty_printi(page_allocator_get_locked_amount());
    tty_print("Locked Memory Size (MB)");
    tty_printi((page_allocator_get_locked_amount() * 4096) / 1048576);
    tty_print("Locked Memory Size (GB)");
    tty_printi((page_allocator_get_locked_amount() * 4096) / 1073741824);


    tty_put('\n');

    tty_print("Unlocked Page Amount");
    tty_printi(page_allocator_get_unlocked_amount());
    tty_print("Unlocked Memory Size (MB)");
    tty_printi((page_allocator_get_unlocked_amount() * 4096) / 1048576);
    tty_print("Unlocked Memory Size (GB)");
    tty_printi((page_allocator_get_unlocked_amount() * 4096) / 1073741824);

    tty_put('\n');

    for (int i = 0; i < 5; i++)
    {
        void* newPage = page_allocator_request();

        tty_print("New Page");
        tty_printi(newPage);  

        tty_put('\n');
    }

    //tty_print("Test 1");

    //FileContent* file = fopen("/fonts/zap-vga16.psf", "r");

    //tty_print("FILE");
    //tty_printi(file);
    
    //tty_print("Test 2");

    //char buffer[64];
    //fread(buffer, 1, 64, file);*/

    //fclose(file);

    /*tty_print("Magic should be 1078");
    uint16_t magic = *((uint16_t*)((void*)file->Data));
    char string[64];
    itoa(magic, string);
    tty_print(string);*/

    while (1)
    {
        asm("hlt");
    }
}