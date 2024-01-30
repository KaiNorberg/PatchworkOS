#include "master.h"

#include "idt/idt.h"
#include "gdt/gdt.h"
#include "apic/apic.h"
#include "tty/tty.h"
#include "utils/utils.h"
#include "string/string.h"

#include "master/pic/pic.h"
#include "master/interrupts/interrupts.h"

static uint8_t apicId;
static Idt idt;

extern void master_loop();

void master_init()
{
    tty_start_message("Master initializing");

    write_msr(MSR_WORKER_ID, -1);

    apicId = local_apic_id();
    master_idt_populate(&idt);

    tty_end_message(TTY_MESSAGE_OK);
}

void master_entry()
{
    gdt_load();
    idt_load(&idt);
    
    pic_remap();

    local_apic_init();
    apic_timer_init(1000);

    master_loop();
}

uint8_t master_apic_id()
{
    return apicId;
}

uint8_t is_master()
{
    return ((uint32_t)read_msr(MSR_WORKER_ID) == (uint32_t)-1);
}