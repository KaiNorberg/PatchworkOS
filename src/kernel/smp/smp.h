#pragma once

#include <stdint.h>

#include "tss/tss.h"
#include "process/process.h"

#define SMP_MAX_CPU_AMOUNT 32

#define SMP_TRAMPOLINE_LOADED_START ((void*)0x8000)

#define SMP_TRAMPOLINE_DATA_PAGE_DIRECTORY 0x8FF0
#define SMP_TRAMPOLINE_DATA_STACK_TOP 0x8FE0
#define SMP_TRAMPOLINE_DATA_ENTRY 0x8FD0

#define IPI_BASE 0x90
#define IPI_HALT 0x90
#define IPI_YIELD 0x91

typedef struct
{
    uint8_t present;
    
    uint8_t id; 
    uint8_t localApicId;
} Cpu;

extern void smp_trampoline_start();
extern void smp_trampoline_end();

void smp_init();

Cpu* smp_cpu(uint8_t cpuId);

Cpu* smp_current_cpu();

uint8_t smp_cpu_amount();

void smp_send_ipi(Cpu* cpu, uint8_t vector);

void smp_send_ipi_to_all(uint8_t vector);

void smp_send_ipi_to_others(uint8_t vector);