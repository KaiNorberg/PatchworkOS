#pragma once

#include "cpu/trap.h"

#include <sys/proc.h>
#include <time.h>

typedef struct cpu cpu_t;

/**
 * @brief System time and timers.
 * @defgroup kernel_timer timer
 * @ingroup kernel_sched
 *
 * The timer subsystem provides kernel time management.
 *
 */

/**
 * @brief Maximum amount of timer callbacks.
 * @ingroup kernel_timer
 */
#define TIMER_MAX_CALLBACK 16

/**
 * @brief Timer callback function type.
 * @ingroup kernel_timer
 */
typedef void (*timer_callback_t)(trap_frame_t* trapFrame, cpu_t* self);

/**
 * @brief Per-CPU system time context.
 * @ingroup kernel_timer
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
 * @brief System time initialization.
 * @ingroup kernel_timer
 *
 */
void timer_init(void);

/**
 * @brief Initialize per-CPU timer.
 * @ingroup kernel_timer
 */
void timer_cpu_init(void);

/**
 * @brief Time since boot.
 * @ingroup kernel_timer
 *
 * @return clock_t The time in nanoseconds since boot.
 */
clock_t timer_uptime(void);

/**
 * @brief The unix epoch.
 * @ingroup kernel_timer
 *
 * @return time_t The amount of seconds since the unix epoch.
 */
time_t timer_unix_epoch(void);

/**
 * @brief Handle timer trap.
 * @ingroup kernel_timer
 *
 * @param trapFrame The current trap frame.
 * @param self The current cpu.
 */
void timer_trap_handler(trap_frame_t* trapFrame, cpu_t* self);

/**
 * @brief Subscribe to timer traps.
 * @ingroup kernel_timer
 *
 * @param callback The callback function to be called on timer traps.
 */
void timer_subscribe(timer_callback_t callback);

/**
 * @brief Unsubscribe from timer traps.
 * @ingroup kernel_timer
 *
 * @param callback The callback function to be removed from timer traps.
 */
void timer_unsubscribe(timer_callback_t callback);

/**
 * @brief Schedule a one-shot timer trap.
 * @ingroup kernel_timer
 *
 * The `timer_one_shot()` function sets the per-cpu timer to generate a trap after the specified timeout.
 * Multiple calls with different timeouts will result in the timer being set for the shortest requested timeout, this
 * will be reset after a timer trap.
 *
 * The idea is that every system that wants timer traps calls the `timer_one_shot()` function with their desired
 * timeout and then when the timer trap occurs they check if their desired time has been reached, if it has they do what
 * they need to do, else they call the function once more respecifying their desired timeout, and we repeat the process.
 * This does technically result in some uneeded checks but its a very simply way of effectively eliminating the need to
 * care about timer related race conditions.
 *
 * @param self The currently running cpu.
 * @param uptime The time since boot, we need to specify this as an argument to avoid inconsistency in the
 * timeout/deadline calculations.
 * @param timeout The desired timeout.
 */
void timer_one_shot(cpu_t* self, clock_t uptime, clock_t timeout);

/**
 * @brief Trigger timer trap on cpu.
 * @ingroup kernel_cpu_smp
 *
 * The `timer_notify()` function triggers the timer interrupt on the specified cpu.
 *
 * @param cpu The destination cpu.
 */
void timer_notify(cpu_t* cpu);