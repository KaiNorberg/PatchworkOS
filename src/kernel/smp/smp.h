#pragma once

#include <stdint.h>

#define SMP_TRAMPOLINE_LOAD_START ((void*)0x1000)

#define SMP_TRAMPOLINE_DATA_PAGE_DIRECTORY 0x500
#define SMP_TRAMPOLINE_DATA_GDT 0x510
#define SMP_TRAMPOLINE_DATA_IDT 0x520

typedef struct
{
    uint8_t present;

    uint8_t id;
    uint8_t lapicId;
} Cpu;

extern void smp_trampoline_start();
extern void smp_trampoline_end();

void smp_init(void* entry);

uint8_t smp_get_cpu_amount();