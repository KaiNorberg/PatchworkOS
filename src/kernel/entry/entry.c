#include "kernel/gdt/gdt.h"
#include "kernel/tty/tty.h"
#include "kernel/idt/idt.h"
#include "kernel/utils/utils.h"
#include "kernel/file_system/file_system.h"
#include "kernel/page_allocator/page_allocator.h"
#include "kernel/multitasking/multitasking.h"
#include "kernel/program_loader/program_loader.h"
#include "kernel/heap/heap.h"

#include "kernel/kernel/kernel.h"

#include "common.h"

#include "kernel/debug/debug.h"

void _start(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r" (cr3));

    tty_print("\n\rLoading program...\n\n\r");

    if (!load_program("/PROGRAMS/test.elf"))
    {
        tty_print("Failed to load program!\n\r");
    }
    
    multitasking_visualize();

    tty_print("Yielding to program...\n\n\r");

    uint64_t rax = SYS_YIELD;
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");

    tty_print("\nBack in the main task!\n\n\r");

    multitasking_visualize();

    while (1)
    {
        asm volatile("hlt");
    }
}