#include "time.h"

#include <stdatomic.h>

#include "irq/irq.h"
#include "apic/apic.h"
#include "hpet/hpet.h"

static _Atomic uint64_t accumulator;

static void time_irq_handler(uint8_t irq)
{
    time_accumulate();
}

void time_init()
{
    apic_timer_init(IRQ_BASE + IRQ_TIMER, TIMER_HZ);
    irq_install_handler(time_irq_handler, IRQ_TIMER);

    accumulator = 0;
    time_accumulate();
}

void time_accumulate()
{
    //Avoids overflow on the hpet counter if counter is 32bit.
    atomic_fetch_add(&accumulator, hpet_read_counter());
    hpet_reset_counter();
}

uint64_t time_seconds()
{
    return time_nanoseconds() / NANOSECONDS_PER_SECOND;
}

uint64_t time_milliseconds()
{
    return time_nanoseconds() / NANOSECONDS_PER_MILLISECOND;
}

uint64_t time_nanoseconds()
{
    return (atomic_load(&accumulator) + hpet_read_counter()) * hpet_nanoseconds_per_tick();
}
