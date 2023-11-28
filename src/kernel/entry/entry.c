#include "kernel/gdt/gdt.h"
#include "kernel/tty/tty.h"
#include "kernel/idt/idt.h"
#include "kernel/utils/utils.h"
#include "kernel/file_system/file_system.h"
#include "kernel/page_allocator/page_allocator.h"

#include "kernel/kernel/kernel.h"

void _start(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

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