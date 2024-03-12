#pragma once

#include <stdint.h>

#include "tss/tss.h"
#include "interrupt_frame/interrupt_frame.h"

#define MAX_CPU_AMOUNT 256

#define IPI_BASE 0x90
#define IPI_HALT 0
#define IPI_START 1
#define IPI_SCHEDULE 2
#define IPI_AMOUNT 3

#define SMP_SEND_IPI_TO_SELF(ipi) asm volatile("int %0" : : "i" (IPI_BASE + ipi))

typedef struct
{
    uint8_t present;
    
    uint8_t id; 
    uint8_t localApicId;    

    Tss* tss;
    void* idleStackTop;
    void* idleStackBottom;
} Cpu;

void smp_init();

void smp_send_ipi(Cpu const* cpu, uint8_t ipi);

void smp_send_ipi_to_others(uint8_t ipi);

uint8_t smp_cpu_amount();

Cpu const* smp_cpu(uint8_t id);

//Must have a corresponding call to smp_put()
Cpu* smp_self();

Cpu* smp_self_unsafe();

Cpu* smp_self_brute();

void smp_put();