#include "time.h"

#include "interrupts/interrupts.h"
#include "tty/tty.h"
#include "hpet/hpet.h"

uint64_t time_seconds()
{
    return time_nanoseconds() / NANOSECONDS_PER_SECOND;
}

uint64_t time_milliseconds()
{
    return time_nanoseconds() / (NANOSECONDS_PER_SECOND / 1000);
}

uint64_t time_nanoseconds()
{
    return hpet_read_counter() * hpet_nanoseconds_per_tick();
}
