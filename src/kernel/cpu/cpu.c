#include "cpu.h"

#include <stdatomic.h>

#include "madt/madt.h"
#include "tty/tty.h"
#include "pmm/pmm.h"
#include "vmm/vmm.h"
#include "apic/apic.h"
#include "hpet/hpet.h"
#include "utils/utils.h"
#include "debug/debug.h"
#include "smp/trampoline/trampoline.h"

static atomic_bool ready;

void cpu_entry()
{    
    PAGE_DIRECTORY_LOAD(vmm_kernel_directory());

    kernel_cpu_init();

    ready = 1;

    while (1)
    {
        asm volatile("sti");
        asm volatile("hlt");
    }
}

uint8_t cpu_init(Cpu* cpu, uint8_t id, uint8_t localApicId)
{
    cpu->present = 1;
    cpu->id = id;
    cpu->localApicId = localApicId;
    cpu->tss = tss_new();
    cpu->ipi = (Ipi){.type = IPI_TYPE_NONE};
    cpu->interruptFrame = 0;

    if (localApicId == local_apic_id())
    {
        return 1;
    }

    smp_trampoline_cpu_setup(cpu);

    local_apic_send_init(localApicId);
    hpet_sleep(10);
    local_apic_send_sipi(localApicId, ((uint64_t)SMP_TRAMPOLINE_PHYSICAL_START) / PAGE_SIZE);

    ready = 0;

    uint64_t timeout = 1000;
    while (!ready) 
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

void cpu_begin_interrupt(InterruptFrame* interruptFrame)
{

}

void cpu_end_interrupt()
{

}