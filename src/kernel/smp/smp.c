#include "smp.h"

#include "tty/tty.h"
#include "apic/apic.h"
#include "string/string.h"
#include "hpet/hpet.h"
#include "page_directory/page_directory.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "utils/utils.h"

uint8_t cpuAmount;
Cpu cpus[255];

void smp_init(void* entry)
{    
    tty_start_message("SMP initializing");

    memset(cpus, 0, sizeof(cpus));
    cpuAmount = 0;
    WRITE_8(0x100, 1);

    Madt* madt = (Madt*)rsdt_lookup("APIC");
    if (madt == 0)
    {
        tty_print("Hardware is incompatible, unable to find Madt");
        tty_end_message(TTY_MESSAGE_ER);
    }

    uint64_t trampolineLength = (uint64_t)smp_trampoline_end - (uint64_t)smp_trampoline_start;
    memcpy(SMP_TRAMPOLINE_LOAD_START, smp_trampoline_start, trampolineLength);

    static GdtDesc gdtDesc;
	gdtDesc.size = sizeof(gdt) - 1;
	gdtDesc.offset = (uint64_t)&gdt;

    static IdtDesc idtDesc;
    idtDesc.size = (sizeof(IdtEntry) * 256) - 1;
    idtDesc.offset = (uint64_t)&idt;

    WRITE_32(SMP_TRAMPOLINE_DATA_PAGE_DIRECTORY, (uint64_t)kernelPageDirectory);
    WRITE_64(SMP_TRAMPOLINE_DATA_GDT, &gdtDesc);
    WRITE_64(SMP_TRAMPOLINE_DATA_IDT, &idtDesc);

    for (MadtRecord* record = madt->records; (uint64_t)record < (uint64_t)madt + madt->header.length; record = (MadtRecord*)((uint64_t)record + record->length))
    {
        if (record->type == MADT_RECORD_TYPE_LAPIC)
        {
            MadtLapicRecord* lapicRecord = (MadtLapicRecord*)record;

            if (!cpus[lapicRecord->cpuId].present)
            {
                cpuAmount++;

                cpus[lapicRecord->cpuId].present = 1;
                cpus[lapicRecord->cpuId].id = lapicRecord->cpuId;
                cpus[lapicRecord->cpuId].lapicId = lapicRecord->lapicId;

                if (lapic_current_cpu() != lapicRecord->cpuId)
                {
                    lapic_send_init(lapicRecord->lapicId);
                    hpet_sleep(10);
                    lapic_send_sipi(lapicRecord->lapicId, 1);
                }
            }
        }
    }
    
    tty_end_message(TTY_MESSAGE_OK);

    tty_print("Cpu Amount: "); tty_printi(smp_get_cpu_amount()); tty_print("\n\r");

    while (1)
    {
        tty_print("Ready Cpu Amount: "); tty_printi(READ_8(0x100)); tty_print("\r");
    }
}

uint8_t smp_get_cpu_amount()
{
    return cpuAmount;
}