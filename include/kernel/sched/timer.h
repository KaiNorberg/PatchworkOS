#pragma once

#include <kernel/cpu/interrupt.h>
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
 *
 * Each CPU has its own timer context, to which timer callbacks can be registered, which will be called on every timer
interrupt. These interrupts are whats called "one-shot" interrupts, meaning that the interrupt will only occur once and
then a new interrupt must be programmed.
 *
 * The way we handle timer interrupts is that each system calls the `timer_one_shot()` function with their desired
timeout and then when the timer interrupt occurs they check if their desired time has been reached, if it has they do
what they need to do, else they call the function once more respecifying their desired timeout, and we repeat the
process. This does technically result in some uneeded checks but its a very simply way of effectively eliminating timer
related race conditions.
 *
 * @{
 */

/**
 * @brief Maximum amount of timer callbacks.
 */
#define TIMER_MAX_CALLBACK 16

/**
 * @brief Timer callback function type.
 */
typedef void (*timer_callback_t)(interrupt_frame_t* frame, cpu_t* self);

/**
 * @brief Per-CPU system time context.
 */
typedef struct
{
    /**
     * The amount of ticks in the owner cpus apic timer that occur every nanosecond, stored using fixed point
     * arithmetic, see `apic_timer_ticks_per_ns()` for more info. Initialized lazily.
     */
    uint64_t apicTicksPerNs;
    /**
     * The next time the owner cpus apic timer will fire, specified in nanoseconds since boot, used in
     * `timer_one_shot()`.
     */
    clock_t nextDeadline;
    /**
     * The registered timer callbacks for the owner cpu.
     */
    timer_callback_t callbacks[TIMER_MAX_CALLBACK];
    lock_t lock;
} timer_cpu_ctx_t;

/**
 * @brief Initialize per-CPU timer context.
 *
 * Must be called on the CPU who owns the context.
 *
 * @param ctx The timer context to initialize.
 */
void timer_cpu_ctx_init(timer_cpu_ctx_t* ctx);

/**
 * @brief Handle timer interrupt.
 *
 * @param frame The current interrupt frame.
 * @param self The current cpu.
 */
void timer_interrupt_handler(interrupt_frame_t* frame, cpu_t* self);

/**
 * @brief Register a callback for timer interrupts.
 *
 * Note that registering a callback only applies to the cpu that the timer context belongs to.
 *
 * @param ctx The timer context that the registration is for.
 * @param callback The callback function to be called on timer interrupts.
 */
void timer_register_callback(timer_cpu_ctx_t* ctx, timer_callback_t callback);

/**
 * @brief Unregister a callback from timer interrupts.
 *
 * Note that unregistering from a callback only applies to the cpu that the timer context belongs to.
 *
 * @param ctx The timer context that the unregistration is for.
 * @param callback The callback function to unregister.
 */
void timer_unregister_callback(timer_cpu_ctx_t* ctx, timer_callback_t callback);

/**
 * @brief Schedule a one-shot timer interrupt.
 *
 * Sets the per-cpu timer to generate a interrupt after the specified timeout.
 * Multiple calls with different timeouts will result in the timer being set for the shortest requested timeout, this
 * will be reset after a timer interrupt.
 *
 * @param self The currently running cpu.
 * @param uptime The time since boot, we need to specify this as an argument to avoid inconsistency in the
 * timeout/deadline calculations.
 * @param timeout The desired timeout.
 */
void timer_one_shot(cpu_t* self, clock_t uptime, clock_t timeout);

/**
 * @brief Trigger timer interrupt on cpu.
 *
 * Triggers the timer interrupt on the specified cpu.
 *
 * @param cpu The destination cpu.
 */
void timer_notify(cpu_t* cpu);

/**
 * @brief Trigger timer interrupt on self.
 *
 * Triggers the timer interrupt on the current cpu.
 */
void timer_notify_self(void);

/** @} */
