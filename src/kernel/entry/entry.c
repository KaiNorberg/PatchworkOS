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

void task1()
{
    tty_print("Hello from task1!\n\r");

    uint64_t rax = SYS_YIELD;
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");
}

void task2()
{
    tty_print("Hello from task2, this task will exit!\n\n\r");
    multitasking_visualize();
    
    uint64_t rax = SYS_EXIT;
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");
}

/*
    tty_print("\n\rLoading program..\n\n\r");

    Program* program = load_program("/PROGRAMS/test.elf", bootInfo);

    void* stackBottom = page_allocator_request();
    uint64_t stackSize = 0x1000;

    create_task(program->Header.Entry, program->AddressSpace, program->StackBottom, program->StackSize);

    tty_print("Yielding...\n\n\r");

    //yield();

    uint64_t rax = 5555555555555555555;
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");
*/

void _start(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r" (cr3));

    tty_print("\n\rCreating task1...\n\n\r");
    void* stackBottom = page_allocator_request();
    uint64_t stackSize = 0x1000;
    create_task(task1, (VirtualAddressSpace*)cr3, stackBottom, stackSize);

    multitasking_visualize();

    tty_print("Creating task2...\n\n\r");
    create_task(task2, (VirtualAddressSpace*)cr3, stackBottom, stackSize);

    multitasking_visualize();

    tty_print("Yielding...\n\n\r");

    uint64_t rax = SYS_YIELD;
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");
    
    tty_print("Back in the main task!\n\n\r");

    multitasking_visualize();

    uint64_t test = 1234;
    asm volatile("jmp %0" : : "r" (test));

    while (1)
    {
        asm("hlt");
    }
}