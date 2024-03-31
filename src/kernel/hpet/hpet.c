#include "hpet.h"

#include "tty/tty.h"
#include "utils/utils.h"
#include "vmm/vmm.h"

static Hpet* hpet;
static uintptr_t hpetAddress;

static uint64_t hpetPeriod;

void hpet_init(void)
{    
    tty_start_message("HPET initializing");

    hpet = (Hpet*)rsdt_lookup("HPET");
    tty_assert(hpet != NULL, "Hardware is incompatible, unable to find HPET");

    hpetAddress = (uintptr_t)vmm_map((void*)hpet->address, 1, PAGE_FLAG_WRITE);
    hpetPeriod = hpet_read(HPET_GENERAL_CAPABILITIES) >> HPET_COUNTER_CLOCK_OFFSET;

    hpet_write(HPET_GENERAL_CONFIG, HPET_CONFIG_DISABLE);
    hpet_write(HPET_MAIN_COUNTER_VALUE, 0);
    hpet_write(HPET_GENERAL_CONFIG, HPET_CONFIG_ENABLE);

    tty_end_message(TTY_MESSAGE_OK);
}

uint64_t hpet_read_counter(void)
{
    return hpet_read(HPET_MAIN_COUNTER_VALUE);
}

void hpet_reset_counter(void)
{
    hpet_write(HPET_GENERAL_CONFIG, HPET_CONFIG_DISABLE);
    hpet_write(HPET_MAIN_COUNTER_VALUE, 0);
    hpet_write(HPET_GENERAL_CONFIG, HPET_CONFIG_ENABLE);
}

uint64_t hpet_nanoseconds_per_tick(void)
{
    return hpetPeriod / 1000000;
}

void hpet_write(uint64_t reg, uint64_t value)
{
    WRITE_64(hpetAddress + reg, value);
}

uint64_t hpet_read(uint64_t reg)
{
    return READ_64(hpetAddress + reg);
}

void hpet_sleep(uint64_t milliseconds)
{
    uint64_t target = hpet_read(HPET_MAIN_COUNTER_VALUE) + (milliseconds * 1000000000000) / hpetPeriod;
    while (!(hpet_read(HPET_MAIN_COUNTER_VALUE) >= target))
    {
        asm volatile("pause");
    }
}

void hpet_nanosleep(uint64_t nanoseconds)
{
    uint64_t target = hpet_read(HPET_MAIN_COUNTER_VALUE) + (nanoseconds * 1000000) / hpetPeriod;
    while (!(hpet_read(HPET_MAIN_COUNTER_VALUE) >= target))
    {
        asm volatile("pause");
    }
}