#include "smp.h"

#include "tty/tty.h"
#include "apic/apic.h"
#include "string/string.h"
#include "hpet/hpet.h"
#include "page_directory/page_directory.h"
#include "page_allocator/page_allocator.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "utils/utils.h"
#include "tss/tss.h"
#include "kernel/kernel.h"
#include "madt/madt.h"
#include "scheduler/scheduler.h"

uint8_t cpuAmount;
atomic_int readyCpuAmount;

Cpu cpus[SMP_MAX_CPU_AMOUNT];

void smp_cpu_entry()
{    
    kernel_cpu_init();

    readyCpuAmount++;
    
    while (1)
    {
        asm volatile("hlt");
    }
}

uint8_t smp_enable_cpu(uint8_t cpuId, uint8_t localApicId)
{
    if (cpus[cpuId].present)
    {
        return 0;
    }

    cpuAmount++;

    cpus[cpuId].present = 1;
    cpus[cpuId].id = cpuId;
    cpus[cpuId].localApicId = localApicId;

    if (local_apic_current_cpu() != cpuId)
    {
        WRITE_64(SMP_TRAMPOLINE_DATA_STACK_TOP, (void*)tss_get(cpuId)->rsp0);

        local_apic_send_init(localApicId);
        hpet_sleep(10);
        local_apic_send_sipi(localApicId, ((uint64_t)SMP_TRAMPOLINE_LOADED_START) / 0x1000);

        uint64_t timeout = 1000;
        while (cpuAmount != readyCpuAmount) 
        {
            hpet_sleep(1);
            timeout--;
            if (timeout == 0)
            {
                return 0;
            }
        }
    }

    return 1;
}

void smp_init()
{    
    tty_start_message("SMP initializing");

    memset(cpus, 0, sizeof(Cpu) * SMP_MAX_CPU_AMOUNT);
    cpuAmount = 0;
    readyCpuAmount = 1;

    uint64_t trampolineLength = (uint64_t)smp_trampoline_end - (uint64_t)smp_trampoline_start;

    void* oldData = page_allocator_request();
    memcpy(oldData, SMP_TRAMPOLINE_LOADED_START, trampolineLength);

    memcpy(SMP_TRAMPOLINE_LOADED_START, smp_trampoline_start, trampolineLength);

    WRITE_32(SMP_TRAMPOLINE_DATA_PAGE_DIRECTORY, (uint64_t)kernelPageDirectory);
    WRITE_64(SMP_TRAMPOLINE_DATA_ENTRY, smp_cpu_entry);

    LocalApicRecord* record = madt_first_record(MADT_RECORD_TYPE_LOCAL_APIC);
    while (record != 0)
    {
        if (MADT_LOCAL_APIC_RECORD_IS_ENABLEABLE(record))
        {
            if (!smp_enable_cpu(record->cpuId, record->localApicId))
            {
                tty_print("CPU "); tty_printi(record->cpuId); tty_print(" failed to start!");
                tty_end_message(TTY_MESSAGE_ER);
            }
        }

        record = madt_next_record(record, MADT_RECORD_TYPE_LOCAL_APIC);
    }

    memcpy(SMP_TRAMPOLINE_LOADED_START, oldData, trampolineLength);
    page_allocator_unlock_page(oldData);

    tty_end_message(TTY_MESSAGE_OK);
}

Cpu* smp_cpu(uint8_t cpuId)
{
    return &cpus[cpuId];
}

Cpu* smp_current_cpu()
{
    return &cpus[local_apic_current_cpu()];
}

uint8_t smp_cpu_amount()
{
    return cpuAmount;
}

Ipi smp_receive_ipi()
{        
    Cpu* cpu = smp_current_cpu();     

    if (cpu->ipi.type != IPI_TYPE_NONE)
    {            
        return cpu->ipi;
    }
    else
    {
        return (Ipi){.type = IPI_TYPE_NONE};
    }
}

void smp_send_ipi(Cpu* cpu, Ipi ipi)
{  
    if (cpu->present)
    {
        cpu->ipi = ipi;
        local_apic_send_ipi(cpu->localApicId, IPI_VECTOR);        
    }
}

void smp_send_ipi_to_all(Ipi ipi)
{
    smp_send_ipi_to_others(ipi);
    smp_send_ipi(smp_current_cpu(), ipi);
}

void smp_send_ipi_to_others(Ipi ipi)
{
    for (uint64_t cpuId = 0; cpuId < SMP_MAX_CPU_AMOUNT; cpuId++)
    {        
        Cpu* cpu = smp_cpu(cpuId);     

        if (cpu->present && cpu != smp_current_cpu())
        {
            smp_send_ipi(cpu, ipi);
        }
    }
}