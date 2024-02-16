#include "interrupts.h"

#include "tty/tty.h"
#include "apic/apic.h"
#include "debug/debug.h"
#include "utils/utils.h"
#include "page_allocator/page_allocator.h"
#include "ipi/ipi.h"
#include "worker_pool/worker_pool.h"
#include "tty/tty.h"

#include "worker/syscall/syscall.h"

extern uint64_t _workerInterruptsStart;
extern uint64_t _workerInterruptsEnd;
extern PageDirectory* workerPageDirectory;

extern void* workerVectorTable[IDT_VECTOR_AMOUNT];

void worker_idt_populate(Idt* idt)
{
    workerPageDirectory = kernelPageDirectory;

    for (uint16_t vector = 0; vector < IDT_VECTOR_AMOUNT; vector++) 
    {        
        idt_set_vector(idt, (uint8_t)vector, workerVectorTable[vector], IDT_RING0, IDT_INTERRUPT_GATE);
    }        
    
    idt_set_vector(idt, SYSCALL_VECTOR, workerVectorTable[SYSCALL_VECTOR], IDT_RING3, IDT_INTERRUPT_GATE);
}

void worker_interrupts_map(PageDirectory* pageDirectory)
{
    void* virtualAddress = (void*)round_down((uint64_t)&_workerInterruptsStart, 0x1000);
    void* physicalAddress = page_directory_get_physical_address(kernelPageDirectory, virtualAddress);
    uint64_t pageAmount = GET_SIZE_IN_PAGES((uint64_t)&_workerInterruptsEnd - (uint64_t)&_workerInterruptsStart);

    page_directory_remap_pages(pageDirectory, virtualAddress, physicalAddress, pageAmount, PAGE_DIR_READ_WRITE);
}

void worker_interrupt_handler(InterruptFrame* interruptFrame)
{            
    if (interruptFrame->vector < IDT_EXCEPTION_AMOUNT)
    {
        worker_exception_handler(interruptFrame);
    }
    else if (interruptFrame->vector == SYSCALL_VECTOR)
    {
        syscall_handler(interruptFrame);
    }
    else if (interruptFrame->vector == IPI_VECTOR)
    {
        worker_ipi_handler(interruptFrame);
    }
}

void worker_ipi_handler(InterruptFrame* interruptFrame)
{
    Ipi ipi = worker_receive_ipi();

    switch (ipi.type)
    {
    case IPI_WORKER_HALT:
    {
        asm volatile("cli");
        while (1)
        {
            asm volatile("hlt");
        }
    }
    break;
    case IPI_WORKER_SCHEDULE:
    {
        Worker* worker = worker_self();

        scheduler_acquire(worker->scheduler);
        scheduler_schedule(worker->scheduler, interruptFrame);
        scheduler_release(worker->scheduler);
    }
    break;
    }        
    
    local_apic_eoi();
}

void worker_exception_handler(InterruptFrame* interruptFrame)
{
    tty_acquire();
    debug_exception(interruptFrame, "Worker Exception");
    tty_release();

    asm volatile("cli");
    while (1)
    {
        asm volatile("hlt");
    }
}