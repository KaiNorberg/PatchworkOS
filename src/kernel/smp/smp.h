#pragma once

#include <stdint.h>

#include "tss/tss.h"
#include "pmm/pmm.h"
#include "scheduler/scheduler.h"
#include "interrupt_frame/interrupt_frame.h"

#define CPU_MAX_AMOUNT 255
#define CPU_IDLE_STACK_SIZE PAGE_SIZE

#define IPI_BASE 0x90
#define IPI_HALT 0
#define IPI_START 1
#define IPI_SCHEDULE 2
#define IPI_AMOUNT 3

#define SMP_SEND_IPI_TO_SELF(ipi) asm volatile("int %0" : : "i" (IPI_BASE + ipi))

typedef struct
{
    uint8_t id;
    uint8_t localApicId;
    Tss tss;
    Scheduler scheduler;
    uint8_t interruptsEnabled;
    uint64_t interruptDepth;
    uint64_t cliAmount;
    uint8_t idleStack[CPU_IDLE_STACK_SIZE];
} Cpu;

void smp_init(void);

uint8_t smp_initialized();

void smp_send_ipi(Cpu const* cpu, uint8_t ipi);

void smp_send_ipi_to_others(uint8_t ipi);

uint8_t smp_cpu_amount(void);

Cpu const* smp_cpu(uint8_t id);

//Must have a corresponding call to smp_put()
Cpu* smp_self(void);

Cpu* smp_self_unsafe(void);

Cpu* smp_self_brute(void);

void smp_put(void);