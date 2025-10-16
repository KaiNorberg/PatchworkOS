#pragma once

#include "config.h"
#include "sched/sched.h"
#include "sched/timer.h"
#include "sched/wait.h"
#include "trap.h"
#include "tss.h"
#include "utils/statistics.h"

#include <stdint.h>

/**
 * @brief CPU
 * @defgroup kernel_cpu CPU
 * @ingroup kernel
 *
 * @{
 */

/**
 * @brief Maximum number of CPUs supported.
 */
#define CPU_MAX UINT8_MAX

/**
 * @brief ID of the bootstrap CPU.
 */
#define CPU_BOOTSTRAP_ID 0

/**
 * @brief Type used to identify a CPU.
 */
typedef uint16_t cpuid_t;

/**
 * @brief CPU structure.
 * @struct cpu_t
 *
 * We allocate the stack buffers inside the `cpu_t` structure to avoid memory allocation during early boot.
 */
typedef struct cpu
{
    cpuid_t id;
    uint8_t lapicId;
    uint32_t trapDepth;
    tss_t tss;
    cli_ctx_t cli;
    statistics_cpu_ctx_t stat;
    stack_pointer_t exceptionStack;
    stack_pointer_t doubleFaultStack;
    timer_ctx_t timer;
    wait_cpu_ctx_t wait;
    sched_cpu_ctx_t sched;
    uint8_t exceptionStackBuffer[CONFIG_EXCEPTION_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t doubleFaultStackBuffer[CONFIG_EXCEPTION_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
} cpu_t;

/**
 * @brief Initializes a CPU structure.
 *
 * @param cpu The CPU structure to initialize.
 * @param id The ID of the CPU.
 * @param lapicId The Local APIC ID of the CPU.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t cpu_init(cpu_t* cpu, cpuid_t id, uint8_t lapicId);

/**
 * @brief Starts a CPU.
 *
 * The trampoline must already be initialized using `trampoline_init()` before calling this function as the CPU will
 * start executing in `trampoline_start()` which eventually calls `cpu_entry()`.
 *
 * @param cpu The CPU to start.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t cpu_start(cpu_t* cpu);

/** @} */
