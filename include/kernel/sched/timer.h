#pragma once

#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/irq.h>
#include <kernel/sync/lock.h>

#include <sys/proc.h>
#include <time.h>

typedef struct cpu cpu_t;

/**
 * @brief Per-CPU timers.
 * @defgroup kernel_timer Timer subsystem
 * @ingroup kernel_sched
 *
 * The timer subsystem is responsible for managing per-CPU timers which are responsible for generating timer interrupts.
These interrupts are whats called "one-shot" interrupts, meaning that the interrupt will only occur once and then a new
interrupt must be programmed.
 *
 * ## Timer Interrupts
 *
 * The way we handle timer interrupts is that each subsystem that relies on the timer calls the `timer_set()` function
with their desired deadline and then, when the timer interrupt occurs, the timer interrupt is acknowledged and the usual
interrupt handling process continues. For example, the scheduler and wait system will check if they need to do anything.
 *
 * Both the scheduler and the wait system can now call `timer_set()` again if they need to schedule another timer
interrupt or if the time they requested has not yet occurred.
 *
 * This does technically result in some uneeded checks but its a very simply way of effectively eliminating timer
related race conditions.
 *
 * ## Timer Sources
 *
 * The actual timer interrupts are provided by "timer sources" (`timer_source_t`), which are registered by modules. Each
source registers itself with a estimate of its precision, the timer subsystem then chooses the source with the highest
precision as the active timer source.
 *
 * @{
 */

/**
 * @brief Per-CPU system time context.
 */
typedef struct
{
    /**
     * The next time the owner cpus apic timer will fire, specified in nanoseconds since boot, used in
     * `timer_set()`.
     */
    clock_t deadline;
} timer_cpu_ctx_t;

/**
 * @brief Maximum amount of timer sources.
 */
#define TIMER_MAX_SOURCES 4

/**
 * @brief Timer source structure.
 * @struct timer_source_t
 */
typedef struct
{
    const char* name;
    clock_t precision;
    /**
     * @brief Should set the one-shot timer to fire after the specified timeout.
     *
     * Should panic on failure, as failing to set a timer will almost certainly result in the system hanging.
     *
     * @param virt The virtual IRQ to use for the timer interrupt, usually `VECTOR_TIMER`.
     * @param uptime The current uptime in nanoseconds.
     * @param timeout The desired timeout in nanoseconds, if `CLOCKS_NEVER`, the timer should be disabled.
     */
    void (*set)(irq_virt_t virt, clock_t uptime, clock_t timeout);
    void (*ack)(cpu_t* cpu);
    void (*eoi)(cpu_t* cpu);
} timer_source_t;

/**
 * @brief Initialize per-CPU timer context.
 *
 * Must be called on the CPU who owns the context.
 *
 * @param ctx The timer context to initialize.
 */
void timer_cpu_ctx_init(timer_cpu_ctx_t* ctx);

/**
 * @brief Acknowledge a timer interrupt and send EOI.
 *
 * @param frame The interrupt frame of the timer interrupt.
 * @param self The CPU on which the timer interrupt was received.
 */
void timer_ack_eoi(interrupt_frame_t* frame, cpu_t* self);

/**
 * @brief Register a timer source.
 *
 * @param source The timer source to register.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOSPC`: No more timer sources can be registered.
 * - Other errors as returned by `irq_handler_register()`.
 */
uint64_t timer_source_register(const timer_source_t* source);

/**
 * @brief Unregister a timer source.
 *
 * @param source The timer source to unregister, or `NULL` for no-op.
 */
void timer_source_unregister(const timer_source_t* source);

/**
 * @brief Get the amount of registered timer sources.
 *
 * @return The amount of registered timer sources.
 */
uint64_t timer_source_amount(void);

/**
 * @brief Schedule a one-shot timer interrupt on the current CPU.
 *
 * Sets the per-cpu timer to generate a interrupt after the specified timeout.
 *
 * Multiple calls with different timeouts will result in the timer being set for the shortest requested timeout, this
 * will be reset after a timer interrupt.
 *
 * The reason we need to specify the current uptime, is not just as a slight optimization, but also to ensure the caller
 * knows exactly what time they are scheduling the timer for, as the uptime could change between the caller reading the
 * time and this function setting the timer, resulting in very subtle bugs or race conditions.
 *
 * @note Will never set the timeout to be less than `CONFIG_MIN_TIMER_TIMEOUT` to avoid spamming the CPU with timer
 * interrupts.
 *
 * @param uptime The time since boot, we need to specify this as an argument to avoid inconsistency in the
 * timeout/deadline calculations.
 * @param deadline The desired deadline.
 */
void timer_set(clock_t uptime, clock_t deadline);

/** @} */
