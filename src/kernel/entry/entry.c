#include "kernel/gdt/gdt.h"
#include "kernel/tty/tty.h"
#include "kernel/idt/idt.h"
#include "kernel/utils/utils.h"
#include "kernel/file_system/file_system.h"
#include "kernel/page_allocator/page_allocator.h"
#include "kernel/multitasking/multitasking.h"

#include "kernel/heap/heap.h"

#include "kernel/kernel/kernel.h"

void task1()
{
    tty_print("Hello from task1!\n\r");
    multitasking_visualize();
    yield();
}

void task2()
{
    tty_print("Hello from task2!\n\r");
    multitasking_visualize();
    yield();
}

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
    }

    tty_print("Allocating memory...\n\r");

    tty_print("\n\rHeap Visualization:\n\r");
    heap_visualize();
    tty_print("\n\r");

    for (int i = 0; i < 10; i++)
    {
        kfree(addresses[i]);
    }

    tty_print("Freeing memory...\n\r");

    tty_print("\n\rHeap Visualization:\n\r");
    heap_visualize();
    //tty_print("\n\r");

    uint64_t cr3 = 0;
    asm volatile("mov %%cr3, %%rax; mov %%rax, %0;":"=m"(cr3)::"%rax");

    multitasking_visualize();

    tty_print("Creating task1...\n\r");
    create_task(task1, (VirtualAddressSpace*)cr3);

    multitasking_visualize();

    tty_print("Creating task2...\n\r");
    create_task(task2, (VirtualAddressSpace*)cr3);

    multitasking_visualize();

    tty_print("Yielding...\n\n\r");

    yield();

    tty_print("Back in the main task!\n\r");

    multitasking_visualize();

    while (1)
    {
        asm volatile("HLT");
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