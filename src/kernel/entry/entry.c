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

void _start(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    tty_print("\n\rLoading programs...\n\n\r");

    for (int i = 0; i < 1; i++)
    {
        if (!load_program("/programs/test/test.elf"))
        {
            tty_print("Failed to load program!\n\r");
        }
    }

    multitasking_visualize();

    tty_print("Yielding...\n\n\r");

    uint64_t rax = SYS_YIELD;
    asm volatile("movq %0, %%rax;" "int $0x80" : : "r"(rax));

    tty_print("\nBack in the main task!\n\n\r");

    multitasking_visualize();

    while (1)
    {
        asm volatile("hlt");
    }
}