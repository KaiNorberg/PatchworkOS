#include "startup.h"

#include <stdatomic.h>

#include "kernel/kernel.h"
#include "tty/tty.h"
#include "vmm/vmm.h"
#include "pmm/pmm.h"
#include "apic/apic.h"
#include "hpet/hpet.h"
#include "madt/madt.h"
#include "heap/heap.h"
#include "smp/trampoline/trampoline.h"

static uint8_t ready;

static inline uint64_t cpu_init(Cpu* cpu, uint8_t id, uint8_t localApicId)
{
    cpu->id = id;
    cpu->localApicId = localApicId;
    cpu->idleStack = kmalloc(CPU_IDLE_STACK_SIZE);
    tss_init(&cpu->tss);
    scheduler_init(&cpu->scheduler);

    ready = false;

    if (localApicId == local_apic_id())
    {
        return 0;
    }

    smp_trampoline_cpu_setup(cpu);

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
            return ERR;
        }
    }

    return 0;
}

void smp_entry(void)
{
    space_load(0);

    kernel_cpu_init();

    ready = true;
    while (true)
    {
        asm volatile("sti");
        asm volatile("hlt");
    }
}

void smp_startup(Cpu cpus[])
{
    uint8_t newId = 0;

    LocalApicRecord* record = madt_first_record(MADT_RECORD_TYPE_LOCAL_APIC);
    while (record != NULL)
    {
        if (LOCAL_APIC_RECORD_GET_FLAG(record, LOCAL_APIC_RECORD_FLAG_ENABLEABLE))
        {
            uint8_t id = newId;
            newId++;

            if (cpu_init(&cpus[id], id, record->localApicId) == ERR)
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