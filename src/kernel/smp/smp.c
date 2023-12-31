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

#include "atomic/atomic.h"

uint8_t cpuAmount;
atomic_uint8_t readyCpuAmount;

Cpu cpus[SMP_MAX_CPU_AMOUNT];

void smp_cpu_entry()
{    
    tty_print("Hello from cpu "); tty_printi(lapic_current_cpu()); tty_print("! ");

    kernel_cpu_init();

    readyCpuAmount++;
    
    hpet_sleep(1);

    while (1)
    {
        asm volatile("hlt");
    }
}

uint8_t smp_enable_cpu(uint8_t cpuId, uint8_t lapicId)
{
    if (cpuId >= SMP_MAX_CPU_AMOUNT)
    {
        return 0;
    }

    if (cpus[cpuId].present)
    {
        return 0;
    }

    cpuAmount++;

    cpus[cpuId].present = 1;
    cpus[cpuId].id = cpuId;
    cpus[cpuId].lapicId = lapicId;

    if (lapic_current_cpu() != cpuId)
    {
        WRITE_64(SMP_TRAMPOLINE_DATA_STACK_TOP, (uint64_t)page_allocator_request() + 0x1000);

        lapic_send_init(lapicId);
        hpet_sleep(10);
        lapic_send_sipi(lapicId, ((uint64_t)SMP_TRAMPOLINE_LOADED_START) / 0x1000);

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

    Madt* madt = (Madt*)rsdt_lookup("APIC");
    if (madt == 0)
    {
        tty_print("Hardware is incompatible, unable to find MADT");
        tty_end_message(TTY_MESSAGE_ER);
    }

    uint64_t trampolineLength = (uint64_t)smp_trampoline_end - (uint64_t)smp_trampoline_start;

    void* oldData = page_allocator_request();
    memcpy(oldData, SMP_TRAMPOLINE_LOADED_START, trampolineLength);

    memcpy(SMP_TRAMPOLINE_LOADED_START, smp_trampoline_start, trampolineLength);

    WRITE_32(SMP_TRAMPOLINE_DATA_PAGE_DIRECTORY, (uint64_t)kernelPageDirectory);
    WRITE_64(SMP_TRAMPOLINE_DATA_ENTRY, smp_cpu_entry);

    for (MadtRecord* record = madt->records; (uint64_t)record < (uint64_t)madt + madt->header.length; record = (MadtRecord*)((uint64_t)record + record->length))
    {
        if (record->type == MADT_RECORD_TYPE_LAPIC)
        {
            MadtLapicRecord* lapicRecord = (MadtLapicRecord*)record;

            if (MADT_LAPIC_RECORD_IS_ENABLEABLE(lapicRecord))
            {
                if (!smp_enable_cpu(lapicRecord->cpuId, lapicRecord->lapicId))
                {
                    tty_print("CPU "); tty_printi(lapicRecord->cpuId); tty_print(" failed to start!");
                    tty_end_message(TTY_MESSAGE_ER);
                }
            }
        }
    }

    memcpy(SMP_TRAMPOLINE_LOADED_START, oldData, trampolineLength);
    page_allocator_unlock_page(oldData);

    tty_end_message(TTY_MESSAGE_OK);
}

Cpu* smp_current_cpu()
{
    return &cpus[lapic_current_cpu()];
}

uint8_t smp_cpu_amount()
{
    return cpuAmount;
}