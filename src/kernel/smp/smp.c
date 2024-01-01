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

#include "atomic/atomic.h"

uint8_t cpuAmount;
atomic_uint8_t readyCpuAmount;

Cpu cpus[SMP_MAX_CPU_AMOUNT];

void smp_cpu_entry()
{    
    tty_print("Hello from cpu "); tty_printi(local_apic_current_cpu()); tty_print("! ");

    kernel_cpu_init();

    readyCpuAmount++;
    
    hpet_sleep(1);

    while (1)
    {
        asm volatile("hlt");
    }
}

uint8_t smp_enable_cpu(uint8_t cpuId, uint8_t localApicId)
{
    if (cpuId >= SMP_MAX_CPU_AMOUNT || cpus[cpuId].present)
    {
        return 0;
    }

    cpuAmount++;

    cpus[cpuId].present = 1;
    cpus[cpuId].id = cpuId;
    cpus[cpuId].localApicId = localApicId;

    if (local_apic_current_cpu() != cpuId)
    {
        WRITE_64(SMP_TRAMPOLINE_DATA_STACK_TOP, (uint64_t)page_allocator_request() + 0x1000);

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

    LocalApicRecord* record = (LocalApicRecord*)madt_first_record(MADT_RECORD_TYPE_LOCAL_APIC);
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

        record = (LocalApicRecord*)madt_next_record((MadtRecord*)record, MADT_RECORD_TYPE_LOCAL_APIC);
    }

    memcpy(SMP_TRAMPOLINE_LOADED_START, oldData, trampolineLength);
    page_allocator_unlock_page(oldData);

    tty_end_message(TTY_MESSAGE_OK);
}

Cpu* smp_current_cpu()
{
    return &cpus[local_apic_current_cpu()];
}

uint8_t smp_cpu_amount()
{
    return cpuAmount;
}