#include "time.h"

#include "hpet/hpet.h"

static uint64_t accumulator;

void time_init()
{
    accumulator = 0;
    time_accumulate();
}

void time_accumulate()
{
    //Avoids overflow on the hpet counter if counter is 32bit.
    accumulator += hpet_read_counter();
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
    return (accumulator + hpet_read_counter()) * hpet_nanoseconds_per_tick();
}
