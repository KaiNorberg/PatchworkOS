#pragma once

#include "defs.h"
#include "mem/pmm.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "sync/futex.h"
#include "syscall.h"
#include "trap.h"
#include "tss.h"
#include "utils/statistics.h"

#define CPU_MAX_AMOUNT 255
#define CPU_IDLE_STACK_SIZE PAGE_SIZE

#define IPI_QUEUE_MAX 4

typedef void (*ipi_t)(trap_frame_t*);

typedef struct
{
    ipi_t ipis[IPI_QUEUE_MAX];
    uint8_t readIndex;
    uint8_t writeIndex;
    lock_t lock;
} ipi_queue_t;

typedef struct cpu
{
    uint8_t id;
    uint8_t lapicId;
    uint64_t trapDepth;
    tss_t tss;
    cli_ctx_t cli;
    sched_ctx_t sched;
    wait_cpu_ctx_t wait;
    statistics_cpu_ctx_t stat;
    ipi_queue_t queue;
    uint8_t idleStack[CPU_IDLE_STACK_SIZE];
} cpu_t;

void smp_bootstrap_init(void);

void smp_others_init(void);

void smp_entry(void);

void smp_halt_others(void);

void smp_ipi_receive(trap_frame_t* trapFrame, cpu_t* self);

void smp_send(cpu_t* cpu, ipi_t ipi);

void smp_send_all(ipi_t ipi);

void smp_send_others(ipi_t ipi);

uint8_t smp_cpu_amount(void);

cpu_t* smp_cpu(uint8_t id);

cpu_t* smp_self_unsafe(void);

cpu_t* smp_self_brute(void);

// Must have a corresponding call to smp_put()
cpu_t* smp_self(void);

void smp_put(void);
