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

void user_space_entry()
{
    tty_print("Hello from user space!\n\r");
    
    while (1)
    {

    }
}

void _start(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    /*tty_print("\n\r");

    heap_visualize();
    tty_print("Locked pages: "); tty_printi(page_allocator_get_locked_amount()); tty_print("\n\r");

    tty_print("\n\rLoading programs (This is really slow for now)...\n\n\r");

    for (int i = 0; i < 5; i++)
    {    
        load_program("/programs/test/test.elf"); //This is really slow
    }

    multitasking_visualize();*/

    load_program("/programs/test/test.elf"); //This is really slow

    tty_print("\n\rJumping to user space...\n\n\r");
    
    multitasking_yield_to_user_space();

    tty_print("\nBack in the main task, if you see this something has gone very wrong!\n\n\r");

    /*multitasking_visualize();

    heap_visualize();
    tty_print("Locked pages: "); tty_printi(page_allocator_get_locked_amount()); tty_print("\n\n\r");*/

    while (1)
    {
        asm volatile("hlt");
    }
}