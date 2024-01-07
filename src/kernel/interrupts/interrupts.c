#include "interrupts.h"

#include "kernel/kernel.h"
#include "io/io.h"
#include "tty/tty.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "scheduler/scheduler.h"
#include "debug/debug.h"
#include "string/string.h"
#include "time/time.h"
#include "heap/heap.h"
#include "idt/idt.h"
#include "page_allocator/page_allocator.h"
#include "smp/smp.h"
#include "apic/apic.h"
#include "spin_lock/spin_lock.h"

#include "../common.h"

extern uint64_t interruptVectorsStart;
extern uint64_t interruptVectorsEnd;

extern PageDirectory* interruptPageDirectory;

const char* exception_strings[32] = 
{
    "Division Fault",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "Floating Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception"
};

void interrupts_init()
{
    tty_start_message("Interrupt vectors initializing");    

    interruptPageDirectory = kernelPageDirectory;

    tty_end_message(TTY_MESSAGE_OK);
}

void interrupts_enable()
{    
    asm volatile ("sti");
}

void interrupts_disable()
{
    asm volatile ("cli");
}

void interrupt_vectors_map(PageDirectory* pageDirectory)
{
    void* virtualAddress = (void*)round_down((uint64_t)&interruptVectorsStart, 0x1000);
    void* physicalAddress = page_directory_get_physical_address(kernelPageDirectory, virtualAddress);
    uint64_t pageAmount = GET_SIZE_IN_PAGES((uint64_t)&interruptVectorsEnd - (uint64_t)&interruptVectorsStart);

    page_directory_remap_pages(pageDirectory, virtualAddress, physicalAddress, pageAmount, PAGE_DIR_READ_WRITE);
}

void interrupt_handler(InterruptFrame* interruptFrame)
{           
    if (interruptFrame->vector < IRQ_BASE)
    {
        exception_handler(interruptFrame);
    }
    else if (interruptFrame->vector >= IRQ_BASE && interruptFrame->vector <= IRQ_BASE + IRQ_AMOUNT)
    {    
        irq_handler(interruptFrame);
    }
    else if (interruptFrame->vector == SYSCALL_VECTOR)
    {
        syscall_handler(interruptFrame);
    } 
    else if (interruptFrame->vector >= IPI_BASE)
    {
        ipi_handler(interruptFrame);
    }
}

void irq_handler(InterruptFrame* interruptFrame)
{
    uint64_t irq = interruptFrame->vector - IRQ_BASE;

    switch (irq)
    {
    case IRQ_TIMER:
    {
        scheduler_tick(interruptFrame);
    }
    break;
    default:
    {
        //Not implemented
    }
    break;
    }        

    local_apic_eoi();
}

void ipi_handler(InterruptFrame* interruptFrame)
{        
    switch (interruptFrame->vector)
    {
    case IPI_HALT:
    {
        interrupts_disable();

        while (1)
        {
            asm volatile("hlt");
        }
    }
    break;
    case IPI_YIELD:
    {
        scheduler_acquire();

        scheduler_yield(interruptFrame);

        scheduler_release();
    }
    break;
    default:
    {
        //Not implemented
    }
    break;
    }

    local_apic_eoi();
}

void exception_handler(InterruptFrame* interruptFrame)
{   
    smp_send_ipi_to_others(IPI_HALT);

    debug_exception(interruptFrame, "Exception");

    //Ipi ipi = IPI_CREATE(IPI_TYPE_HALT);
    smp_send_ipi_to_others(IPI_HALT);

    while (1)
    {
        asm volatile("hlt");
    }
}