#include "worker.h"

#include "page_directory/page_directory.h"
#include "utils/utils.h"
#include "pmm/pmm.h"
#include "vmm/vmm.h"
#include "apic/apic.h"
#include "hpet/hpet.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "worker_pool/worker_pool.h"
#include "worker/trampoline/trampoline.h"
#include "worker/scheduler/scheduler.h"

uint8_t worker_init(Worker* worker, uint8_t id, uint8_t apicId)
{
    worker->present = 1;
    worker->running = 0;
    worker->id = id;
    worker->apicId = apicId;

    worker->tss = tss_new();
    worker->ipi = (Ipi){.type = IPI_WORKER_NONE};
    worker->scheduler = scheduler_new();

    worker_trampoline_worker_setup(worker);

    local_apic_send_init(apicId);
    hpet_sleep(10);
    local_apic_send_sipi(apicId, ((uint64_t)WORKER_TRAMPOLINE_PHYSICAL_START) / PAGE_SIZE);

    uint64_t timeout = 1000;
    while (!worker->running) 
    {
        hpet_sleep(1);
        timeout--;
        if (timeout == 0)
        {
            return 0;
        }
    }

    return 1;
}

void worker_entry()
{
    PAGE_DIRECTORY_LOAD(vmm_kernel_directory());

    local_apic_init();

    Worker* worker = worker_self_brute();
    write_msr(MSR_WORKER_ID, worker->id);
    
    gdt_load();
    gdt_load_tss(worker->tss);

    idt_load(worker_idt_get());

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
