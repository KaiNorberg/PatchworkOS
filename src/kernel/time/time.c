#include "time.h"

#include "tty/tty.h"
#include "hpet/hpet.h"

static uint64_t nanoseconds;

void time_init()
{
    tty_start_message("Time initializing");

    nanoseconds = 0;
    time_tick();

    tty_end_message(TTY_MESSAGE_OK);
}

void time_tick()
{
    nanoseconds += hpet_read_counter() * hpet_nanoseconds_per_tick();
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
    return nanoseconds + hpet_read_counter() * hpet_nanoseconds_per_tick();
}
