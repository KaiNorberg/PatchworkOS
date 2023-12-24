#include "time.h"

#include "interrupts/interrupts.h"
#include "tty/tty.h"

uint64_t ticksSinceBoot;

void time_init()
{    
    tty_start_message("Time initializing");

    ticksSinceBoot = 0;

    tty_end_message(TTY_MESSAGE_OK);
}

uint64_t time_get_seconds()
{
    return ticksSinceBoot / TICKS_PER_SECOND;
}

uint64_t time_get_milliseconds()
{
    return (ticksSinceBoot * 1000) / (TICKS_PER_SECOND);
}

uint64_t time_get_tick()
{
    return ticksSinceBoot;
}

void time_tick()
{
    ticksSinceBoot++;
}