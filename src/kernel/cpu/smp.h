#pragma once

#include "drivers/systime/systime.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "trap.h"
#include "tss.h"
#include "utils/statistics.h"

#include <stdint.h>

/**
 * @brief Symmetric multi processing.
 * @ingroup kernel_cpu
 * @defgroup kernel_cpu_smp SMP
 *
 */

#define SMP_CPU_MAX UINT8_MAX

#define IPI_QUEUE_MAX 4

typedef uint8_t cpuid_t;

typedef struct cpu
{
    cpuid_t id;
    uint8_t lapicId;
    bool isBootstrap;
    uint64_t trapDepth;
    tss_t tss;
    cli_ctx_t cli;
    systime_ctx_t systime;
    sched_cpu_ctx_t sched;
    wait_cpu_ctx_t wait;
    statistics_cpu_ctx_t stat;
} cpu_t;

void smp_bootstrap_init(void);

void smp_others_init(void);

void smp_entry(cpuid_t id);

void smp_halt_others(void);

/**
 * @brief Trigger trap on cpu.
 * @ingroup kernel_cpu_smp
 *
 * The `smp_notify()` function, sends an ipi that does nothing to the specified cpu, effectively causing a trap on the
 * cpu allowing it to for example schedule, etc.
 *
 * @param cpu The destination cpu.
 */
void smp_notify(cpu_t* cpu);

uint8_t smp_cpu_amount(void) PURE_FUNC;

cpu_t* smp_cpu(uint8_t id) PURE_FUNC;

cpu_t* smp_self_unsafe(void) PURE_FUNC;

// Must have a corresponding call to smp_put()
cpu_t* smp_self(void);

void smp_put(void);
