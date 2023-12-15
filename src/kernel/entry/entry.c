#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "utils/utils.h"
#include "file_system/file_system.h"
#include "page_allocator/page_allocator.h"
#include "multitasking/multitasking.h"
#include "program_loader/program_loader.h"
#include "heap/heap.h"

#include "kernel/kernel.h"

#include "../common.h"

#include "debug/debug.h"

#include "string/string.h"

void _start(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    /*void* addresses[20];

    page_allocator_visualize();
    heap_visualize();

    tty_print("Allocating small memory...\n\r");
    for (int i = 0; i < 10; i++)
    {
        addresses[i] = kmalloc(16);
    }

    page_allocator_visualize();
    heap_visualize();

    tty_print("Allocating large memory...\n\r");
    for (int i = 10; i < 20; i++)
    {
        addresses[i] = kmalloc(0x2000);     
    }

    page_allocator_visualize();
    heap_visualize();

    tty_print("Freeing memory...\n\r");
    for (int i = 0; i < 20; i++)
    {
        kfree(addresses[i]);   
    }

    page_allocator_visualize();
    heap_visualize();*/
    
    tty_print("\n\rLoading program1...\n\r");
    load_program("/programs/test1/test1.elf");

    tty_print("Loading program2...\n\r");
    load_program("/programs/test2/test2.elf");

    tty_print("Jumping to user space...\n\n\r");
    
    enable_interrupts();

    multitasking_yield_to_user_space();

    tty_print("\nBack in the main task, if you see this something has gone very wrong!\n\n\r");
    
    while (1)
    {
        asm volatile("hlt");
    }
}