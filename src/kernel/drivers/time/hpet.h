#pragma once

#include <sys/proc.h>

#include "acpi/acpi.h"

#define HPET_COUNTER_CLOCK_OFFSET 0x20

#define HPET_GENERAL_CAPABILITIES 0x000
#define HPET_GENERAL_CONFIG 0x010
#define HPET_MAIN_COUNTER_VALUE 0x0F0

#define HPET_CFG_ENABLE 0b1
#define HPET_CFG_DISABLE 0b0
#define HPET_CFG_LEGACY_MODE 0b10

#define HPET_TIMER_CONFIG_CAPABILITY(n) (0x100 + 0x20 * n)
#define HPET_TIMER_COMPARATOR(n) (0x108 + 0x20 * n)

typedef struct PACKED
{
    sdt_t header;
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
} hpet_t;

void hpet_init(void);

uint64_t hpet_nanoseconds_per_tick(void);

uint64_t hpet_read_counter(void);

void hpet_reset_counter(void);

void hpet_write(uint64_t reg, uint64_t value);

uint64_t hpet_read(uint64_t reg);

void hpet_wait(clock_t nanoseconds);
