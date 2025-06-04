#pragma once

#include "cpu/trap.h"
#include "defs.h"

#include <sys/proc.h>
#include <time.h>

/**
 * @brief System time and timers.
 * @defgroup kernel_systime systime
 * @ingroup kernel
 *
 * The systime subsystem provides kernel time management.
 *
 */

#define RTC_HZ 2

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

typedef struct cpu cpu_t;

/**
 * @brief Per-CPU system time context.
 * @ingroup kernel_systime
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
     * `systime_timer_one_shot()`.
     */
    clock_t nextDeadline;
} systime_ctx_t;

/**
 * @brief System time initialization.
 * @ingroup kernel_systime
 *
 * The `systime_init()` function, initializes time tracking using the cmos and rtc.
 *
 */
void systime_init(void);

/**
 * @brief Time since boot.
 * @ingroup kernel_systime
 *
 * @return clock_t The time in nanoseconds since boot.
 */
clock_t systime_uptime(void);

/**
 * @brief The unix epoch.
 * @ingroup kernel_systime
 *
 * @return time_t The amount of seconds since the unix epoch.
 */
time_t systime_unix_epoch(void);

/**
 * @brief Initialize per-CPU timer.
 * @ingroup kernel_systime
 */
void systime_timer_init(void);

/**
 * @brief Handle timer trap.
 * @ingroup kernel_systime
 *
 * @param trapFrame The current trap frame.
 * @param self The current cpu.
 */
void systime_timer_trap(trap_frame_t* trapFrame, cpu_t* self);

/**
 * @brief Schedule a one-shot timer trap.
 * @ingroup kernel_systime
 *
 * The `systime_timer_one_shot()` function sets the per-cpu timer to generate a trap after the specified timeout.
 * Multiple calls with different timeouts will result in the timer being set for the shortest requested timeout, this
 * will be reset after a timer trap.
 *
 * The idea is that every system that wants timer traps calls the `systime_timer_one_shot()` function with their desired
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
void systime_timer_one_shot(cpu_t* self, clock_t uptime, clock_t timeout);