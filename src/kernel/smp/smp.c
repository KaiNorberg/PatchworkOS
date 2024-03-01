#include "smp.h"

#include <libc/string.h>

#include "smp/trampoline/trampoline.h"
#include "madt/madt.h"
#include "tty/tty.h"
#include "pmm/pmm.h"
#include "vmm/vmm.h"
#include "apic/apic.h"
#include "hpet/hpet.h"
#include "utils/utils.h"
#include "debug/debug.h"

static Cpu cpus[MAX_CPU_AMOUNT];
static uint8_t cpuAmount;
 
void smp_init()
{
    tty_start_message("SMP initializing");

    memset(cpus, 0, sizeof(Cpu) * MAX_CPU_AMOUNT);
    cpuAmount = 0;

    smp_trampoline_setup();

    LocalApicRecord* record = madt_first_record(MADT_RECORD_TYPE_LOCAL_APIC);
    while (record != 0)
    {
        if (LOCAL_APIC_RECORD_GET_FLAG(record, LOCAL_APIC_RECORD_FLAG_ENABLEABLE))
        {
            uint8_t id = cpuAmount;
            cpuAmount++;

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

    smp_trampoline_cleanup();

    tty_end_message(TTY_MESSAGE_OK);
}

void smp_send_ipi(Cpu* cpu, Ipi ipi)
{
    cpu->ipi = ipi;
    local_apic_send_ipi(cpu->localApicId, IPI_VECTOR);
}

Ipi smp_receive_ipi()
{
    Cpu* self = smp_self();

    Ipi temp = self->ipi;
    self->ipi = (Ipi){.type = IPI_TYPE_NONE};

    return temp;
}

uint8_t smp_cpu_amount()
{
    return cpuAmount;
}

Cpu* smp_cpu(uint8_t id)
{
    return &cpus[id];
}

Cpu* smp_self()
{
    uint64_t id = read_msr(MSR_CPU_ID);
    return &cpus[id];
}

Cpu* smp_self_brute()
{
    uint8_t localApicId = local_apic_id();
    for (uint16_t i = 0; i < cpuAmount; i++)
    {
        Cpu* cpu = smp_cpu((uint8_t)i);

        if (cpu->present && cpu->localApicId == localApicId)
        {
            return cpu;
        }
    }    

    debug_panic("Unable to find worker");
    return 0;
}