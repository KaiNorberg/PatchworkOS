#include "entry.h"

#include "kernel/gdt/gdt.h"
#include "kernel/tty/tty.h"
#include "kernel/idt/idt.h"
#include "kernel/utils/utils.h"
#include "kernel/file_system/file_system.h"
#include "kernel/page_allocator/page_allocator.h"
#include "kernel/page_table/page_table.h"

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
    tty_init(bootInfo->Screenbuffer, bootInfo->PSFFonts[0]);

    tty_print("Hello from the kernel!\n\r");

    tty_start_message("Page allocator initializing");
    page_allocator_init(bootInfo->MemoryMap, bootInfo->Screenbuffer);
    tty_end_message(TTY_MESSAGE_OK);

    tty_start_message("Page table initializing");
    page_table_init(bootInfo->Screenbuffer);
    tty_end_message(TTY_MESSAGE_OK);

    tty_clear();

    tty_print("Paging has been initialized\n\r");

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

    tty_print("Total Page Amount\n\r");
    tty_printi(page_allocator_get_total_amount());
    tty_print("\n\r");
    tty_print("Total Memory Size (MB)\n\r");
    tty_printi((page_allocator_get_total_amount() * 4096) / 1048576);
    tty_print("\n\r");
    tty_print("Total Memory Size (GB)\n\r");
    tty_printi((page_allocator_get_total_amount() * 4096) / 1073741824);
    tty_print("\n\r");

    tty_print("\n\r");

    tty_print("Locked Page Amount\n\r");
    tty_printi(page_allocator_get_locked_amount());
    tty_print("\n\r");
    tty_print("Locked Memory Size (MB)\n\r");
    tty_printi((page_allocator_get_locked_amount() * 4096) / 1048576);
    tty_print("\n\r");
    tty_print("Locked Memory Size (GB)\n\r");
    tty_printi((page_allocator_get_locked_amount() * 4096) / 1073741824);
    tty_print("\n\r");

    tty_print("\n\r");

    tty_print("Unlocked Page Amount\n\r");
    tty_printi(page_allocator_get_unlocked_amount());
    tty_print("\n\r");
    tty_print("Unlocked Memory Size (MB)\n\r");
    tty_printi((page_allocator_get_unlocked_amount() * 4096) / 1048576);
    tty_print("\n\r");
    tty_print("Unlocked Memory Size (GB)\n\r");
    tty_printi((page_allocator_get_unlocked_amount() * 4096) / 1073741824);
    tty_print("\n\r");

    tty_print("\n\r");

    for (int i = 0; i < 2; i++)
    {
        uint64_t* newPage = page_allocator_request();

        tty_print("New Page\n\r");
        tty_printi((uint64_t)newPage);
        tty_print("\n\r");  

        tty_print("Array\n\r");
        newPage[0] = 0;
        newPage[1] = 5;
        newPage[2] = 10;
        newPage[3] = 15;
        newPage[4] = 20;
        newPage[5] = 25;

        for (int j = 0; j < 6; j++)
        {
            tty_printi(newPage[j]);
            tty_print("\n\r");
        }

        tty_print("\n\r");

        page_allocator_unlock_page(newPage);
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