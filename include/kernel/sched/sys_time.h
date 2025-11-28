#pragma once

#include <kernel/cpu/interrupt.h>
#include <kernel/sync/lock.h>

#include <sys/proc.h>
#include <time.h>

typedef struct cpu cpu_t;

/**
 * @brief System time.
 * @defgroup kernel_sys_time Time subsystem
 * @ingroup kernel_sched
 *
 * The sys time subsystem is responsible for providing a consistent system wide time keeping.
 *
 * System wide time is provided via "system time sources", which are provided in modules. Each source registers itself
 * with a estimate of its precision, the system time subsystem then chooses the two sources, one for uptime and one for
 * unix epoch, with the best precision.
 *
 * @{
 */

/**
 * @brief Maximum amount of system timer sources.
 */
#define SYS_TIME_MAX_SOURCES 8

/**
 * @brief System time source.
 * @struct sys_time_source_t
 */
typedef struct
{
    const char* name;
    clock_t precision;
    clock_t (*read_ns)(void);
    time_t (*read_epoch)(void);
} sys_time_source_t;

/**
 * @brief Register a system timer source.
 *
 * @param source The timer source to register.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOSPC`: No more timer sources can be registered.
 */
uint64_t sys_time_register_source(const sys_time_source_t* source);

/**
 * @brief Unregister a system timer source.
 *
 * @param source The timer source to unregister, or `NULL` for no-op.
 */
void sys_time_unregister_source(const sys_time_source_t* source);

/**
 * @brief Time since boot.
 *
 * @return The time in nanoseconds since boot.
 */
clock_t sys_time_uptime(void);

/**
 * @brief The unix epoch.
 *
 * @return The amount of seconds since the unix epoch.
 */
time_t sys_time_unix_epoch(void);

/**
 * @brief Wait for a specified number of nanoseconds.
 *
 * This function uses a busy-wait loop, making it highly CPU inefficient, but its useful during early
 * initialization or when you are unable to block the current thread.
 *
 * @param nanoseconds The number of nanoseconds to wait.
 */
void sys_time_wait(clock_t nanoseconds);

/** @} */
