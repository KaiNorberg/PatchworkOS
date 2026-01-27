#pragma once

#include <kernel/cpu/interrupt.h>
#include <kernel/sync/lock.h>

#include <sys/proc.h>
#include <sys/status.h>
#include <time.h>

typedef struct cpu cpu_t;

/**
 * @brief Clock subsystem.
 * @defgroup kernel_sched_clock Clock subsystem.
 * @ingroup kernel_sched
 *
 * The clock subsystem is responsible for providing a consistent system wide time keeping.
 *
 * System wide time is provided via "clock sources", which are provided in modules. Each source registers itself with a
 * estimate of its precision, the clock subsystem then chooses the two sources, one for uptime and one for unix epoch,
 * with the best precision.
 *
 * @{
 */

/**
 * @brief Maximum amount of system timer sources.
 */
#define CLOCK_MAX_SOURCES 8

/**
 * @brief Clock source structure.
 * @struct clock_source_t
 */
typedef struct
{
    const char* name;
    clock_t precision;
    clock_t (*read_ns)(void);
    time_t (*read_epoch)(void);
} clock_source_t;

/**
 * @brief Register a system timer source.
 *
 * @param source The timer source to register.
 * @return An appropriate status value.
 */
status_t clock_source_register(const clock_source_t* source);

/**
 * @brief Unregister a system timer source.
 *
 * @param source The timer source to unregister, or `NULL` for no-op.
 */
void clock_source_unregister(const clock_source_t* source);

/**
 * @brief Retrieve the time in nanoseconds since boot.
 *
 * @return The time in nanoseconds since boot.
 */
clock_t clock_uptime(void);

/**
 * @brief Retrieve the seconds since the unix epoch.
 *
 * @return The amount of seconds since the unix epoch.
 */
time_t clock_epoch(void);

/**
 * @brief Wait for a specified number of nanoseconds.
 *
 * This function uses a busy-wait loop, making it highly CPU inefficient, but its useful during early
 * initialization or when you are unable to block the current thread.
 *
 * @param nanoseconds The number of nanoseconds to wait.
 */
void clock_wait(clock_t nanoseconds);

/** @} */
