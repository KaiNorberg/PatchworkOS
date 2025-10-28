#pragma once

#include "cpu/interrupt.h"
#include "sync/lock.h"

#include <time.h>

typedef struct cpu cpu_t;

/**
 * @brief Performance Statistics driver.
 * @defgroup kernel_drivers_statistics Statistics
 * @ingroup kernel_drivers
 *
 * The Performance Statistics driver is exposed in the `/dev/stat` directory. Below is an overview of the files in this
 * directory.
 *
 * ## Cpu Statistics
 *
 * The `/dev/stat/cpu` file contains per-CPU statistics in the following format:
 * ```
 * cpu idle_clocks active_clocks interrupt_clocks
 * cpu0 123456 789012 345678
 * cpu1 234567 890123 456789
 * ...
 * cpuN 345678 901234 567890
 * ```
 *
 * ## Memory Statistics
 *
 * The `/dev/stat/mem` file contains memory statistics in the following format:
 * ```
 * value kib
 * total 1048576
 * free 524288
 * reserved 131072
 * ```
 *
 * @{
 */

/**
 * @brief Per-CPU statistics context.
 */
typedef struct
{
    clock_t idleClocks;
    clock_t activeClocks;
    clock_t interruptClocks;
    clock_t interruptBegin;
    clock_t interruptEnd;
    lock_t lock;
} statistics_cpu_ctx_t;

/**
 * @brief Initializes a per-CPU statistics context.
 *
 * @param ctx The context to initialize.
 */
void statistics_cpu_ctx_init(statistics_cpu_ctx_t* ctx);

/**
 * @brief Initializes the statistics driver.
 */
void statistics_init(void);

/**
 * @brief Called at the beginning of an interrupt.
 *
 * Will measure the time spent in interrupts and time spent idle/active.
 *
 * @param frame The interrupt frame.
 * @param self The current CPU.
 */
void statistics_interrupt_begin(interrupt_frame_t* frame, cpu_t* self);

/**
 * @brief Called at the end of an interrupt.
 *
 * @param frame The interrupt frame.
 * @param self The current CPU.
 */
void statistics_interrupt_end(interrupt_frame_t* frame, cpu_t* self);

/** @} */
