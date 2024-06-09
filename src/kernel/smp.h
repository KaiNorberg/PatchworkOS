#pragma once

#include "defs.h"
#include "pmm.h"
#include "sched.h"
#include "tss.h"

#define CPU_MAX_AMOUNT 255
#define CPU_IDLE_STACK_SIZE PAGE_SIZE

#define IPI_BASE 0x90
#define IPI_HALT 0
#define IPI_START 1
#define IPI_SCHEDULE 2
#define IPI_AMOUNT 3

#define SMP_SEND_IPI_TO_SELF(ipi) asm volatile("int %0" : : "i"(IPI_BASE + ipi))

typedef struct
{
    uint8_t id;
    uint8_t localApicId;
    uint8_t* idleStack;
    uint64_t trapDepth;
    bool interruptsEnabled;
    uint64_t cliAmount;
    Tss tss;
    Scheduler scheduler;
} Cpu;

void smp_init(void);

void smp_entry(void);

bool smp_initialized(void);

void smp_send_ipi(Cpu const* cpu, uint8_t ipi);

void smp_send_ipi_to_others(uint8_t ipi);

uint8_t smp_cpu_amount(void);

Cpu* smp_cpu(uint8_t id);

// Must have a corresponding call to smp_put()
Cpu* smp_self(void);

Cpu* smp_self_unsafe(void);

Cpu* smp_self_brute(void);

void smp_put(void);