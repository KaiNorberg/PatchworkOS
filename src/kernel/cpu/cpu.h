#pragma once

#include "config.h"
#include "cpu_id.h"
#include "drivers/apic.h"
#include "drivers/statistics.h"
#include "interrupt.h"
#include "sched/sched.h"
#include "sched/timer.h"
#include "sched/wait.h"
#include "tss.h"

#include <stdint.h>

/**
 * @brief CPU
 * @defgroup kernel_cpu CPU
 * @ingroup kernel
 *
 * @{
 */

/**
 * @brief CPU stack canary value.
 *
 * Placed at the bottom of CPU stacks, we then check in the interrupt handler if any of the stacks have overflowed by
 * checking if its canary has been modified.
 */
#define CPU_STACK_CANARY 0x1234567890ABCDEFULL

/**
 * @brief CPU structure.
 * @struct cpu_t
 *
 * We allocate the stack buffers inside the `cpu_t` structure to avoid memory allocation during early boot.
 *
 * Must be stored aligned to a page boundary.
 */
typedef struct cpu
{
    cpuid_t id;
    lapic_id_t lapicId;
    tss_t tss;
    interrupt_ctx_t interrupt;
    statistics_cpu_ctx_t stat;
    timer_ctx_t timer;
    wait_cpu_ctx_t wait;
    sched_cpu_ctx_t sched;
    stack_pointer_t exceptionStack;
    stack_pointer_t doubleFaultStack;
    stack_pointer_t interruptStack;
    uint8_t exceptionStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t doubleFaultStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t interruptStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
} cpu_t;

/**
 * @brief Initializes a CPU structure as part of the boot process.
 *
 * Must be called on the CPU that will be represented by the `cpu` structure.
 *
 * @param cpu The CPU structure to initialize.
 * @param id The ID of the CPU.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t cpu_init(cpu_t* cpu, cpuid_t id);

/**
 * @brief Checks for CPU stack overflows.
 *
 * Checks the canary values at the bottom of each CPU stack and if its been modified panics.
 *
 * @param cpu The CPU to check.
 */
void cpu_stacks_overflow_check(cpu_t* cpu);

/** @} */
