#pragma once

#include "rsdt/rsdt.h"

typedef struct __attribute__((packed)) 
{
    SDTHeader header;
    uint32_t lapicAddress;
    uint32_t flags; 
} MADT;

void apic_init();