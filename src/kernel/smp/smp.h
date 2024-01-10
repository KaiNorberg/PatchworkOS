#pragma once

#include <stdint.h>
#include <stdatomic.h>

#include "tss/tss.h"
#include "process/process.h"
#include "spin_lock/spin_lock.h"

#define IPI_VECTOR 0x90

#define IPI_TYPE_NONE 0x0
#define IPI_TYPE_HALT 0x1
#define IPI_TYPE_YIELD 0x2

#define IPI_CREATE(ipiType) ((Ipi){.type = ipiType})

#define SMP_MAX_CPU_AMOUNT 32

#define SMP_TRAMPOLINE_LOADED_START ((void*)0x8000)

#define SMP_TRAMPOLINE_DATA_PAGE_DIRECTORY 0x8FF0
#define SMP_TRAMPOLINE_DATA_STACK_TOP 0x8FE0
#define SMP_TRAMPOLINE_DATA_ENTRY 0x8FD0

typedef struct
{
    uint8_t type;
} Ipi;

typedef struct
{
    uint8_t present;
    
    uint8_t id; 
    uint8_t localApicId;

    Ipi ipi;
} Cpu;

extern void smp_trampoline_start();
extern void smp_trampoline_end();

void smp_init();

Cpu* smp_cpu(uint8_t cpuId);

Cpu* smp_current_cpu();

uint8_t smp_cpu_amount();

Ipi smp_receive_ipi();

void smp_send_ipi(Cpu* cpu, Ipi ipi);

void smp_send_ipi_to_all(Ipi ipi);

void smp_send_ipi_to_others(Ipi ipi);