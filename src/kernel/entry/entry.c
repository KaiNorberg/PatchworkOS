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
    
    uint64_t rax = SYS_YIELD;
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");
}

void _start(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r" (cr3));

    tty_print("\n\rCreating task1...\n\n\r");
    VirtualAddressSpace* task1AddressSpace = virtual_memory_create();
    for (uint64_t i = 0; i < page_allocator_get_total_amount(); i++)
    {
        virtual_memory_remap(task1AddressSpace, (void*)(i * 0x1000), (void*)(i * 0x1000));
    }
    for (uint64_t i = 0; i < bootInfo->Screenbuffer->Size + 0x1000; i += 0x1000)
    {
        virtual_memory_remap(task1AddressSpace, (void*)((uint64_t)bootInfo->Screenbuffer->Base + i), (void*)((uint64_t)bootInfo->Screenbuffer->Base + i));
    }
    create_task(task1, cr3);

    multitasking_visualize();

    tty_print("Creating task2...\n\n\r");
    VirtualAddressSpace* task2AddressSpace = virtual_memory_create();
    for (uint64_t i = 0; i < page_allocator_get_total_amount(); i++)
    {
        virtual_memory_remap(task2AddressSpace, (void*)(i * 0x1000), (void*)(i * 0x1000));
    }
    for (uint64_t i = 0; i < bootInfo->Screenbuffer->Size + 0x1000; i += 0x1000)
    {
        virtual_memory_remap(task2AddressSpace, (void*)((uint64_t)bootInfo->Screenbuffer->Base + i), (void*)((uint64_t)bootInfo->Screenbuffer->Base + i));
    }
    create_task(task2, cr3);

    multitasking_visualize();

    tty_print("Yielding...\n\n\r");

    //yield();

    uint64_t rax = SYS_YIELD;
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");

    tty_print("Back in the main task!\n\n\r");

    multitasking_visualize();

    while (1)
    {
        asm("hlt");
    }
}