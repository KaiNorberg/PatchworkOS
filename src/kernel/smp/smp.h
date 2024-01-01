#pragma once

#include <stdint.h>

#include "tss/tss.h"

#define SMP_MAX_CPU_AMOUNT 32

#define SMP_TRAMPOLINE_LOADED_START ((void*)0x8000)

#define SMP_TRAMPOLINE_DATA_PAGE_DIRECTORY 0x8FF0
#define SMP_TRAMPOLINE_DATA_STACK_TOP 0x8FE0
#define SMP_TRAMPOLINE_DATA_ENTRY 0x8FD0

typedef struct
{
    uint8_t present;
    
    uint8_t id; 
    uint8_t localApicId;
} Cpu;

extern void smp_trampoline_start();
extern void smp_trampoline_end();

void smp_init();

Cpu* smp_current_cpu();

uint8_t smp_cpu_amount();