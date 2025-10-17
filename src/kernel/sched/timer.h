#pragma once

#include "cpu/interrupt.h"

#include <sys/proc.h>
#include <time.h>

typedef struct cpu cpu_t;

/**
 * @brief System time and timers.
 * @defgroup kernel_timer Time subsystem
 * @ingroup kernel_sched
 *
 * The timer subsystem provides kernel time management.
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
     * @brief The amount of ticks in the owner cpus apic timer that occur every nanosecond, stored using fixed point
     * arithmetic, see `apic_timer_ticks_per_ns()` for more info.
     */
    uint64_t apicTicksPerNs;
    /**
     * @brief The next time the owner cpus apic timer will fire, specified in nanoseconds since boot, used in
     * `timer_one_shot()`.
     */
    clock_t nextDeadline;
} timer_ctx_t;

/**
 * @brief Initialize per-CPU timer context.
 *
 * @param ctx The timer context to initialize.
 */
void timer_ctx_init(timer_ctx_t* ctx);

/**
 * @brief System time initialization.
 *
 */
void timer_init(void);

/**
 * @brief Initialize per-CPU timer.
 */
void timer_cpu_init(void);

/**
 * @brief Time since boot.
 *
 * @return clock_t The time in nanoseconds since boot.
 */
clock_t timer_uptime(void);

/**
 * @brief The unix epoch.
 *
 * @return time_t The amount of seconds since the unix epoch.
 */
time_t timer_unix_epoch(void);

/**
 * @brief Handle timer interrupt.
 *
 * @param frame The current interrupt frame.
 * @param self The current cpu.
 */
void timer_interrupt_handler(interrupt_frame_t* frame, cpu_t* self);

/**
 * @brief Subscribe to timer interrupts.
 *
 * @param callback The callback function to be called on timer interrupts.
 */
void timer_subscribe(timer_callback_t callback);

/**
 * @brief Unsubscribe from timer interrupts.
 *
 * @param callback The callback function to be removed from timer interrupts.
 */
void timer_unsubscribe(timer_callback_t callback);

/**
 * @brief Schedule a one-shot timer interrupt.
 *
 * Sets the per-cpu timer to generate a interrupt after the specified timeout.
 * Multiple calls with different timeouts will result in the timer being set for the shortest requested timeout, this
 * will be reset after a timer interrupt.
 *
 * The idea is that every system that wants timer interrupts calls the `timer_one_shot()` function with their desired
 * timeout and then when the timer interrupt occurs they check if their desired time has been reached, if it has they do
 * what they need to do, else they call the function once more respecifying their desired timeout, and we repeat the
 * process. This does technically result in some uneeded checks but its a very simply way of effectively eliminating the
 * need to care about timer related race conditions.
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
