#include "master.h"

#include "idt/idt.h"
#include "gdt/gdt.h"
#include "apic/apic.h"
#include "tty/tty.h"
#include "time/time.h"
#include "utils/utils.h"

#include "master/dispatcher/dispatcher.h"
#include "master/fast_timer/fast_timer.h"
#include "master/interrupts/interrupts.h"
#include "master/pic/pic.h"
#include "master/slow_timer/slow_timer.h"
#include "master/jobs/jobs.h"

static uint8_t apicId;
static Idt idt;

extern void master_loop();

void master_init()
{
    tty_start_message("Master initializing");

    asm volatile("cli");

    write_msr(MSR_WORKER_ID, -1);

    local_apic_init();
    apicId = local_apic_id();
    
    master_idt_populate(&idt);

    gdt_load();
    idt_load(&idt);
    
    pic_init();
    pic_clear_mask(IRQ_CASCADE);

    dispatcher_init();
    jobs_init();

    fast_timer_init();
    slow_timer_init();

    tty_end_message(TTY_MESSAGE_OK);
}

uint8_t master_apic_id()
{
    return apicId;
}

uint8_t is_master()
{
    return ((uint32_t)read_msr(MSR_WORKER_ID) == (uint32_t)-1);
}