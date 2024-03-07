#pragma once

#include <stdint.h>

#include "tss/tss.h"
#include "interrupt_frame/interrupt_frame.h"

#define MAX_CPU_AMOUNT 256

#define IPI_VECTOR 0x90

#define IPI_TYPE_NONE 0
#define IPI_TYPE_HALT 1
#define IPI_TYPE_SCHEDULE 2

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

    Tss* tss;
    void* idleStackTop;
    void* idleStackBottom;
} Cpu;

void smp_entry();

void smp_init();

void smp_send_ipi(Cpu* cpu, Ipi ipi);

void smp_send_ipi_to_others(Ipi ipi);

void smp_send_ipi_to_self(Ipi ipi);

void smp_send_ipi_to_all(Ipi ipi);

Ipi smp_receive_ipi();

uint8_t smp_cpu_amount();

Cpu const* smp_cpu(uint8_t id);

//Must have a corresponding call to smp_put()
Cpu* smp_self();

Cpu* smp_self_unsafe();

Cpu* smp_self_brute();

void smp_put();