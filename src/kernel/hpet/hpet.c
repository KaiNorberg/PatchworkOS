#include "hpet.h"

#include "tty/tty.h"
#include "debug/debug.h"
#include "page_directory/page_directory.h"
#include "io/io.h"
#include "interrupts/interrupts.h"

HPET* hpet;

uint64_t hpetBase;
uint64_t hpetPeriod;

void hpet_init(uint64_t hertz)
{    
    tty_start_message("HPET initializing");

    hpet = (HPET*)acpi_find("HPET");
    if (hpet == 0)
    {
        debug_panic("Hardware is incompatible, unable to find HPET");
        tty_end_message(TTY_MESSAGE_ER);
    }
    
    hpetBase = hpet->addressStruct.address;
    page_directory_remap(kernelPageDirectory, (void*)hpetBase, (void*)hpetBase, PAGE_DIR_READ_WRITE);

    hpetPeriod = hpet_read(HPET_GENERAL_CAPABILITIES) >> HPET_COUNTER_CLOCK_OFFSET;

    hpet_write(HPET_GENERAL_CONFIG, 0);
    hpet_write(HPET_MAIN_COUNTER_VALUE, 0);
    hpet_write(HPET_GENERAL_CONFIG, 3);

    uint64_t pitTicks = (1000000000000000 / hertz) / hpetPeriod;

    hpet_write(HPET_TIMER_CONFIG_CAPABILITY(0), hpet_read(HPET_TIMER_CONFIG_CAPABILITY(0)) | (1 << 2) | (1 << 3) | (1 << 6));
    hpet_write(HPET_TIMER_COMPARATOR(0), hpet_read(HPET_MAIN_COUNTER_VALUE) + pitTicks);
    hpet_write(HPET_TIMER_COMPARATOR(0), pitTicks);
    
    tty_end_message(TTY_MESSAGE_OK);
}

void hpet_write(uintptr_t reg, uint64_t value)
{
    *((volatile uint64_t*)(hpetBase + reg)) = value;
}

uint64_t hpet_read(uintptr_t reg)
{
    return *((volatile uint64_t*)(hpetBase + reg));
}

void hpet_sleep(int ms)
{
    uint64_t target = hpet_read(HPET_MAIN_COUNTER_VALUE) + (ms * 1000000000000) / hpetPeriod;
    while (!(hpet_read(HPET_MAIN_COUNTER_VALUE) >= target))
    {

    }
}