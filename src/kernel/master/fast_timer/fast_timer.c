#include "fast_timer.h"

#include "apic/apic.h"
#include "master/interrupts/interrupts.h"

void fast_timer_init()
{   
    apic_timer_init(IRQ_BASE + IRQ_FAST_TIMER, FAST_TIMER_HZ);
}

void fast_timer_eoi()
{
    local_apic_eoi();
}