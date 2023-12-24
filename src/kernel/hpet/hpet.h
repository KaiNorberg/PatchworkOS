#pragma once

#include "acpi/acpi.h"

#define HPET_COUNTER_CLOCK_OFFSET 0x20

#define HPET_GENERAL_CAPABILITIES 0x000
#define HPET_GENERAL_CONFIG 0x010
#define HPET_MAIN_COUNTER_VALUE 0x0F0

#define HPET_TIMER_COMPARATOR(n) (0x108 + 0x20 * n)
#define HPET_TIMER_CONFIG_CAPABILITY(n) (0x100 + 0x20 * n)

typedef struct __attribute__((packed))
{
    uint8_t address_space_id;    // 0 - system memory, 1 - system I/O
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved;
    uint64_t address;
} HPETAddressStruct;
 
typedef struct __attribute__((packed))
{   
    DescriptionHeader header;
    uint8_t hardware_rev_id;
    uint8_t comparator_count : 5;
    uint8_t counter_size : 1;
    uint8_t reserved : 1;
    uint8_t legacy_replacement : 1;
    uint16_t pci_vendor_id;
    HPETAddressStruct addressStruct;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} HPET;

void hpet_init(uint64_t hertz);

void hpet_write(uintptr_t reg, uint64_t value);

uint64_t hpet_read(uintptr_t reg);

void hpet_sleep(int ms);