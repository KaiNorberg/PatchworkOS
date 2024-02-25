#include "master.h"

#include "idt/idt.h"
#include "gdt/gdt.h"
#include "apic/apic.h"
#include "tty/tty.h"
#include "utils/utils.h"

#include "master/dispatcher/dispatcher.h"
#include "master/fast_timer/fast_timer.h"
#include "master/slow_timer/slow_timer.h"
#include "master/interrupts/interrupts.h"
#include "master/pic/pic.h"
#include "master/jobs/jobs.h"

static uint8_t localApicId;

extern void master_loop();

void master_init()
{
    tty_start_message("Master initializing");

    write_msr(MSR_WORKER_ID, -1);

    local_apic_init();
    localApicId = local_apic_id();

    gdt_load();
    master_idt_init();

    pic_init();
    pic_clear_mask(IRQ_CASCADE);

    dispatcher_init();
    jobs_init();

    fast_timer_init();
    slow_timer_init();

    tty_end_message(TTY_MESSAGE_OK);
}

uint8_t master_local_apic_id()
{
    return localApicId;
}

uint8_t is_master()
{
    return ((uint32_t)read_msr(MSR_WORKER_ID) == (uint32_t)-1);
}
