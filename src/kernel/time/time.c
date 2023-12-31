#include "time.h"

#include "interrupts/interrupts.h"
#include "tty/tty.h"
#include "hpet/hpet.h"

uint64_t ticksSinceBoot;

void time_init()
{    
    tty_start_message("Time initializing");

    ticksSinceBoot = 0;

    tty_end_message(TTY_MESSAGE_OK);
}

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
    return ticksSinceBoot * (NANOSECONDS_PER_SECOND / TICKS_PER_SECOND);
}

uint64_t time_get_tick()
{
    return ticksSinceBoot;
}

void time_tick()
{
    ticksSinceBoot++;
}