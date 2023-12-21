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

    tty_print("\n\r");
    tty_print("Locked pages: "); tty_printi(page_allocator_get_locked_amount()); tty_print("\n\r");
    tty_print("Locked pages: "); tty_printi((page_allocator_get_locked_amount() * 0x1000) / 0xFFFFF); tty_print(" MB\n\r");
    tty_print("\n\r");

    tty_print("Loading program1...\n\r");
    load_program("/programs/test1/test1.elf");

    tty_print("Loading program2...\n\r");
    load_program("/programs/test2/test2.elf");

    tty_print("\n\r");
    tty_print("Locked pages: "); tty_printi(page_allocator_get_locked_amount()); tty_print("\n\r");
    tty_print("Locked pages: "); tty_printi((page_allocator_get_locked_amount() * 0x1000) / 0xFFFFF); tty_print(" MB\n\r");
    tty_print("\n\r");

    tty_print("rdi = 1 means program 1 is running.\n\r");
    tty_print("rdi = 2 means program 2 is running.\n\n\r");

    tty_print("Jumping to user space...\n\n\r");

    enable_interrupts();

    multitasking_yield_to_user_space();

    tty_print("\nBack in the main task, if you see this something has gone very wrong!\n\n\r");
    
    while (1)
    {
        asm volatile("hlt");
    }
}