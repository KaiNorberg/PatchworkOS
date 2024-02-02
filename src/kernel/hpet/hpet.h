#pragma once

#include "rsdt/rsdt.h"

#define HPET_COUNTER_CLOCK_OFFSET 0x20

#define HPET_GENERAL_CAPABILITIES 0x000
#define HPET_GENERAL_CONFIG 0x010
#define HPET_MAIN_COUNTER_VALUE 0x0F0

#define HPET_CONFIG_ENABLE 0b1
#define HPET_CONFIG_DISABLE 0b0
#define HPET_CONFIG_LEGACY_MODE 0b10

#define HPET_TIMER_CONFIG_CAPABILITY(n) (0x100 + 0x20 * n)
#define HPET_TIMER_COMPARATOR(n) (0x108 + 0x20 * n)

typedef struct __attribute__((packed))
{   
    SdtHeader header;
    uint8_t hardwareRevId;
    uint8_t info;
    uint16_t pciVendorId;
    uint8_t addressSpaceId;
    uint8_t registerBitWidth;
    uint8_t registerBitOffset;
    uint8_t reserved;
    uint64_t address;
    uint8_t hpetNumber;
    uint16_t minimumTick;
    uint8_t pageProtection;
} Hpet;

void hpet_init();

uint64_t hpet_nanoseconds_per_tick();

uint64_t hpet_read_counter();

void hpet_reset_counter();

void hpet_write(uint64_t reg, uint64_t value);

uint64_t hpet_read(uint64_t reg);

void hpet_sleep(uint64_t milliseconds);

void hpet_nanosleep(uint64_t nanoseconds);