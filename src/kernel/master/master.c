#include "master.h"

#include "idt/idt.h"
#include "gdt/gdt.h"
#include "apic/apic.h"
#include "tty/tty.h"
#include "utils/utils.h"
#include "string/string.h"

#include "master/pic/pic.h"
#include "master/interrupts/interrupts.h"

static Master master;

extern void master_loop();

void master_init()
{
    tty_start_message("Master initializing");

    write_msr(MSR_WORKER_ID, -1);

    memset(&master, 0, sizeof(Master));
    master.apicId = local_apic_id();
    master_idt_populate(&master.idt);

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

    gdt_load();
    idt_load(&master.idt);
    
    pic_remap();

    local_apic_init();
    apic_timer_init(2);

    master_loop();
}

uint8_t is_master()
{
    return ((uint32_t)read_msr(MSR_WORKER_ID) == (uint32_t)-1);
}

Master* master_get()
{
    return &master;
}