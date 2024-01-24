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
#include "gdt/gdt.h"
#include "hpet/hpet.h"
#include "kernel_process/kernel_process.h"

#include "../common.h"

extern uint64_t interruptVectorsStart;
extern uint64_t interruptVectorsEnd;

extern PageDirectory* interruptPageDirectory;

void interrupts_init()
{
    tty_start_message("Interrupt vectors initializing");    

    interruptPageDirectory = kernelPageDirectory;

    tty_end_message(TTY_MESSAGE_OK);
}

void interrupts_enable()
{    
    asm volatile("sti");
}

void interrupts_disable()
{
    asm volatile("cli");
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
    else if (interruptFrame->vector == KERNEL_TASK_BLOCK_VECTOR)
    {
        kernel_task_block_handler(interruptFrame);
    }
    else if (interruptFrame->vector == SYSCALL_VECTOR)
    {
        syscall_handler(interruptFrame);
    } 
    else if (interruptFrame->vector == IPI_VECTOR)
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
        local_scheduler_acquire();
        local_scheduler_tick(interruptFrame);
        local_scheduler_release();
    }
    break;
    default:
    {
        //Not implemented
    } 
    break;
    }        

    local_apic_eoi();
    io_pic_eoi(irq);
}

void ipi_handler(InterruptFrame* interruptFrame)
{   
    Ipi ipi = smp_receive_ipi();     
    
    switch (ipi.type)
    {
    case IPI_TYPE_HALT:
    {
        interrupts_disable();

        while (1)
        {
            asm volatile("hlt");
        }
    }
    break;
    case IPI_TYPE_START:
    {
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->cr3 = (uint64_t)kernelPageDirectory;
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->stackPointer = (uint64_t)tss_kernel_stack();
          
        apic_timer_init();
    }
    break;
    default:
    {
        debug_panic("Unknown IPI");
    }
    break;
    }

    local_apic_eoi();
}

void exception_handler(InterruptFrame* interruptFrame)
{        
    tty_acquire();
    tty_print("EXCEPTION - "); tty_print(exceptionStrings[interruptFrame->vector]); tty_print("\n\r");
    tty_release();

    local_scheduler_acquire();             
    local_scheduler_exit();
    local_scheduler_schedule(interruptFrame);
    local_scheduler_release();
}