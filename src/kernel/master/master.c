#include "master.h"

#include "idt/idt.h"
#include "gdt/gdt.h"
#include "apic/apic.h"
#include "interrupts/interrupts.h"
#include "tty/tty.h"
#include "utils/utils.h"

static uint8_t masterApicId;

void master_init()
{
    tty_start_message("Master initializing");

    write_msr(MSR_WORKER_ID, -1);

    masterApicId = local_apic_id();

    tty_end_message(TTY_MESSAGE_OK);
}

void master_entry()
{
    //idt_load();
    //gdt_load();
    //gdt_load_tss(tss_get(smp_current_cpu()->id));

    //local_apic_init();

    //interrupts_enable();

    /*Ipi ipi = 
    {
        .type = IPI_TYPE_START
    };
    sent_ipi_to_workers(ipi);*/
}

uint8_t master_apic_id()
{
    return masterApicId;
}