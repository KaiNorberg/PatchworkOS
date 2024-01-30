#include "worker.h"

#include "page_directory/page_directory.h"
#include "madt/madt.h"
#include "tty/tty.h"
#include "utils/utils.h"
#include "page_allocator/page_allocator.h"
#include "string/string.h"
#include "apic/apic.h"
#include "hpet/hpet.h"
#include "master/master.h"
#include "gdt/gdt.h"
#include "idt/idt.h"

#include "worker_pool/worker_pool.h"

void worker_entry()
{
    Worker* worker = worker_self_brute();
    write_msr(MSR_WORKER_ID, worker->id);
    
    gdt_load();
    gdt_load_tss(worker->tss);
    idt_load(worker_idt_get());

    local_apic_init();

    worker->running = 1;

    while (1)
    {
        asm volatile("sti");
        asm volatile("hlt");
    }
}

Ipi worker_receive_ipi()
{
    Worker* self = worker_self();

    Ipi temp = self->ipi;
    self->ipi = (Ipi){.type = IPI_WORKER_NONE};

    return temp;
}

void worker_send_ipi(Worker* worker, Ipi ipi)
{
    worker->ipi = ipi;
    local_apic_send_ipi(worker->apicId, IPI_VECTOR);
}
