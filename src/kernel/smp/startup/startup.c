#include "startup.h"

#include <stdatomic.h>

#include "kernel/kernel.h"
#include "tty/tty.h"
#include "vmm/vmm.h"
#include "pmm/pmm.h"
#include "apic/apic.h"
#include "hpet/hpet.h"
#include "madt/madt.h"
#include "smp/trampoline/trampoline.h"

static uint8_t ready;

static inline uint8_t cpu_init(Cpu* cpu, uint8_t id, uint8_t localApicId)
{
    cpu->present = 1;
    cpu->id = id;
    cpu->localApicId = localApicId;
    cpu->ipi = (Ipi){.type = IPI_TYPE_NONE};

    cpu->tss = tss_new();
    cpu->idleStackBottom = vmm_allocate(1);
    cpu->idleStackTop = (void*)((uint64_t)cpu->idleStackBottom + PAGE_SIZE);
    cpu->tss->rsp0 = (uint64_t)cpu->idleStackTop;

    if (localApicId == local_apic_id())
    {
        return 1;
    }

    smp_trampoline_cpu_setup(cpu);

    ready = 0;

    local_apic_send_init(localApicId);
    hpet_sleep(10);
    local_apic_send_sipi(localApicId, ((uint64_t)SMP_TRAMPOLINE_PHYSICAL_START) / PAGE_SIZE);

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

void smp_entry()
{    
    page_directory_load(vmm_kernel_directory());

    kernel_cpu_init();

    ready = 1;

    while (1)
    {
        asm volatile("sti");
        asm volatile("hlt");
    }
}

void smp_startup(Cpu cpus[], uint8_t* cpuAmount)
{
    LocalApicRecord* record = madt_first_record(MADT_RECORD_TYPE_LOCAL_APIC);
    while (record != 0)
    {
        if (LOCAL_APIC_RECORD_GET_FLAG(record, LOCAL_APIC_RECORD_FLAG_ENABLEABLE))
        {
            uint8_t id = *cpuAmount;
            (*cpuAmount)++;

            if (!cpu_init(&cpus[id], id, record->localApicId))
            {    
                tty_print("CPU ");
                tty_printi(id);
                tty_print(" failed to start!");
                tty_end_message(TTY_MESSAGE_ER);
            }
        }

        record = madt_next_record(record, MADT_RECORD_TYPE_LOCAL_APIC);
    }
}