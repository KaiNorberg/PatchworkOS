#pragma once

#include <kernel/cpu/interrupt.h>
#include <kernel/sync/lock.h>

#include <time.h>

typedef struct cpu cpu_t;
typedef struct thread thread_t;

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
 * %lu %lu %lu %lu
 * %lu %lu %lu %lu
 * ...
 * %lu %lu %lu %lu
 * ```
 *
 * ## Memory performance
 *
 * The `/dev/perf/mem` file contains memory performance data in the following format:
 * ```
 * total_pages %lu
 * free_pages %lu
 * used_pages %lu
 * ```
 *
 * @see @ref kernel_proc "Process" for per-process performance data.
 *
 * @{
 */

/**
 * @brief Per-CPU performance context.
 * @struct perf_cpu_ctx_t
 */
typedef struct
{
    clock_t activeClocks;
    clock_t interruptClocks;
    clock_t idleClocks;
    clock_t lastUpdate;
    clock_t interruptBegin;
    clock_t interruptEnd;
    bool inInterrupt;
    lock_t lock;
} perf_cpu_ctx_t;

/**
 * @brief Per-Process performance context.
 * @struct stat_process_ctx_t
 */
typedef struct
{
    _Atomic(clock_t) userClocks;   ///< Total user mode CPU time used by this process.
    _Atomic(clock_t) kernelClocks; ///< Total kernel mode CPU time used by this process, does not include interrupt time.
    clock_t startTime;    ///< The time when the process was started.
} perf_process_ctx_t;

/**
 * @brief Per-Thread performance context.
 * @struct perf_thread_ctx_t
 *
 * The thread context tracks tracks the time it spends in and outside of system calls, this is then accumulated into the
 * process performance context.
 */
typedef struct
{
    clock_t syscallBegin; ///< The time the current syscall began. Also used to "skip" time spent in interrupts.
    clock_t syscallEnd;
} perf_thread_ctx_t;

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
 * @brief Initializes a per-thread performance context.
 *
 * @param ctx The context to initialize.
 */
void perf_thread_ctx_init(perf_thread_ctx_t* ctx);

/**
 * @brief Initializes the performance driver.
 */
void perf_init(void);

/**
 * @brief Called at the beginning of an interrupt to update cpu performance data.
 *
 * Must be called with interrupts disabled.
 *
 * @param self The current CPU.
 */
void perf_interrupt_begin(cpu_t* self);

/**
 * @brief Called at the end of an interrupt to update cpu performance data.
 *
 * Must be called with interrupts disabled.
 *
 * @param self The current CPU.
 */
void perf_interrupt_end(cpu_t* self);

/**
 * @brief Called at the beginning of a syscall to update process performance data.
 *
 * Must be called with interrupts disabled.
 */
void perf_syscall_begin(void);

/**
 * @brief Called at the end of a syscall to update process performance data.
 *
 * Must be called with interrupts disabled.
 */
void perf_syscall_end(void);

/** @} */
