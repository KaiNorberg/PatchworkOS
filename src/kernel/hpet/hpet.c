#include "hpet.h"

#include "tty/tty.h"
#include "debug/debug.h"
#include "page_directory/page_directory.h"
#include "io/io.h"
#include "utils/utils.h"

static Hpet* hpet;

static uint64_t hpetPeriod;

void hpet_init()
{    
    tty_start_message("HPET initializing");

    hpet = (Hpet*)rsdt_lookup("HPET");
    if (hpet == 0)
    {
        tty_print("Hardware is incompatible, unable to find HPET");
        tty_end_message(TTY_MESSAGE_ER);
    }
    
    page_directory_remap(kernelPageDirectory, (void*)hpet->address, (void*)hpet->address, PAGE_DIR_READ_WRITE);

    hpetPeriod = hpet_read(HPET_GENERAL_CAPABILITIES) >> HPET_COUNTER_CLOCK_OFFSET;

    hpet_write(HPET_GENERAL_CONFIG, 0);
    hpet_write(HPET_MAIN_COUNTER_VALUE, 0);
    hpet_write(HPET_GENERAL_CONFIG, 1);

    tty_end_message(TTY_MESSAGE_OK);
}

uint64_t hpet_read_counter()
{
    return hpet_read(HPET_MAIN_COUNTER_VALUE);
}

uint64_t hpet_nanoseconds_per_tick()
{
    return hpetPeriod / 1000000;
}

void hpet_write(uint64_t reg, uint64_t value)
{
    WRITE_64(hpet->address + reg, value);
}

uint64_t hpet_read(uint64_t reg)
{
    return READ_64(hpet->address + reg);
}

void hpet_sleep(uint64_t milliseconds)
{
    uint64_t target = hpet_read(HPET_MAIN_COUNTER_VALUE) + (milliseconds * 1000000000000) / hpetPeriod;
    while (!(hpet_read(HPET_MAIN_COUNTER_VALUE) >= target))
    {
        asm volatile("pause");
    }
}