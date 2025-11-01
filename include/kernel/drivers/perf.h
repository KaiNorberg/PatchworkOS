#pragma once

#include <kernel/cpu/interrupt.h>
#include <kernel/sync/lock.h>

#include <time.h>

typedef struct cpu cpu_t;

/**
 * @brief Performance driver.
 * @defgroup kernel_drivers_performance performance
 * @ingroup kernel_drivers
 *
 * The Performance driver is exposed in the `/dev/perf` directory. Below is an overview of the files in this
 * directory.
 *
 * ## Cpu performance
 *
 * The `/dev/perf/cpu` file contains per-CPU performance data in the following format:
 * ```
 * cpu idle_clocks active_clocks interrupt_clocks
 * cpu%d %lu %lu %lu
 * cpu%d %lu %lu %lu
 * ...
 * cpu%d %lu %lu %lu
 * ```
 *
 * ## Memory performance
 *
 * The `/dev/perf/mem` file contains memory performance data in the following format:
 * ```
 * value kib
 * total %lu
 * free %lu
 * reserved %lu
 * ```
 *
 * @see @ref kernel_proc "Process" for per-process performance data.
 *
 * @{
 */

/**
 * @brief Performance switch types.
 * @enum perf_switch_t
 */
typedef enum
{
    PERF_SWITCH_NONE,                   //< No switch.
    PERF_SWITCH_ENTER_SYSCALL,          ///< The current thread is entering a syscall.
    PERF_SWITCH_LEAVE_SYSCALL,          ///< The current thread is leaving a syscall.
    PERF_SWITCH_ENTER_KERNEL_INTERRUPT, ///< The current thread is entering an interrupt that occurred in kernel mode.
    PERF_SWITCH_ENTER_USER_INTERRUPT,   ///< The current thread is entering an interrupt that occurred in user mode.
    PERF_SWITCH_LEAVE_INTERRUPT,        ///< The current thread is leaving an interrupt.
} perf_switch_t;

/**
 * @brief Per-CPU performance context.
 * @struct perf_cpu_ctx_t
 */
typedef struct
{
    clock_t activeClocks;
    clock_t interruptClocks;
    clock_t lastUpdate;
    perf_switch_t lastSwitch;
    lock_t lock;
} perf_cpu_ctx_t;

/**
 * @brief Process performance structure.
 * @struct stat_process_ctx_t
 */
typedef struct
{
    clock_t userClocks;   ///< Total user mode CPU time used by this process.
    clock_t kernelClocks; ///< Total kernel mode CPU time used by this process, does not include interrupt time.
    clock_t startTime;    ///< The time when the process was started.
    lock_t lock;
} perf_process_ctx_t;

/**
 * @brief Initializes a per-CPU performance context, must be called on the CPU that owns the context.
 *
 * @param ctx The context to initialize.
 */
void perf_cpu_ctx_init(perf_cpu_ctx_t* ctx);

/**
 * @brief Initializes a per-process performance context.
 *
 * @param ctx The context to initialize.
 */
void perf_process_ctx_init(perf_process_ctx_t* ctx);

/**
 * @brief Initializes the performance driver.
 */
void perf_init(void);

/**
 * @brief Update performance statistics for the current CPU and process.
 *
 * Must be called with interrupts disabled.
 *
 * @param self The current CPU.
 * @param switchType The type of context switch that occurred.
 */
void perf_update(cpu_t* self, perf_switch_t switchType);

/** @} */
