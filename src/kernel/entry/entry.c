#include "kernel/gdt/gdt.h"
#include "kernel/tty/tty.h"
#include "kernel/idt/idt.h"
#include "kernel/utils/utils.h"
#include "kernel/file_system/file_system.h"
#include "kernel/page_allocator/page_allocator.h"

#include "kernel/heap/heap.h"

#include "kernel/kernel/kernel.h"

void _start(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    void* addresses[10];

    tty_print("Heap Visualization:\n\r");
    heap_visualize();
    tty_print("\n\r");

    for (int i = 0; i < 10; i++)
    {
        addresses[i] = kmalloc(10000);

        tty_print("Allocated address: "); tty_printi((uint64_t)addresses[i]); tty_print("\n\r");
    }

    tty_print("\n\rHeap Visualization:\n\r");
    heap_visualize();
    tty_print("\n\r");

    for (int i = 0; i < 10; i++)
    {
        kfree(addresses[i]);

        tty_print("Freed address: "); tty_printi((uint64_t)addresses[i]); tty_print("\n\r");
    }

    tty_print("\n\rHeap Visualization:\n\r");
    heap_visualize();
    tty_print("\n\r");

    for (int i = 0; i < 10; i++)
    {
        addresses[i] = kmalloc(10000);

        tty_print("Allocated address: "); tty_printi((uint64_t)addresses[i]); tty_print("\n\r");
    }

    tty_print("\n\rHeap Visualization:\n\r");
    heap_visualize();
    tty_print("\n\r");

    for (int i = 0; i < 10; i++)
    {
        kfree(addresses[i]);

        tty_print("Freed address: "); tty_printi((uint64_t)addresses[i]); tty_print("\n\r");
    }

    tty_print("\n\rHeap Visualization:\n\r");
    heap_visualize();
    tty_print("\n\r");

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