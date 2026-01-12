#pragma once

/**
 * @brief Interrupt stack configuration.
 * @ingroup kernel
 * @def CONFIG_INTERRUPT_STACK_PAGES
 *
 * The `CONFIG_INTERRUPT_STACK_PAGES` constant defines the amount of pages that are allocated for the per-CPU interrupt,
 * exception and doubleFault stacks.
 *
 * The interrupt stack can be much much smaller than regular kernel stacks as all interrupt handlers should be as short
 * as possible and allocate as little memory as possible to reduce the amount of time while preemptions are disabled.
 */
#define CONFIG_INTERRUPT_STACK_PAGES 1

/**
 * @brief Kernel stack configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_KERNEL_STACK_PAGES
 *
 * The `CONFIG_MAX_KERNEL_STACK_PAGES` constant defines the maximum amount of pages that are allowed to be allocated for
 * a threads kernel stack, the kernel stack is used while the thread is in kernel space and NOT handling an
 * exception/interrupt.
 *
 */
#define CONFIG_MAX_KERNEL_STACK_PAGES 100

/**
 * @brief User stack configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_USER_STACK_PAGES
 *
 * The `CONFIG_MAX_USER_STACK_PAGES` constant defines the maximum amount of pages that are allowed to be allocated for a
 * threads user stack, the user stack is used while the thread is in user space.
 *
 */
#define CONFIG_MAX_USER_STACK_PAGES 100

/**
 * @brief Maximum file descriptor configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_FD
 *
 * The `CONFIG_MAX_FD` constant defines the maximum amount of file descriptors that a process is allowed to have open.
 *
 */
#define CONFIG_MAX_FD 64

/**
 * @brief Serial logging configuration.
 * @ingroup kernel
 * @def CONFIG_LOG_SERIAL
 *
 * The `CONFIG_LOG_SERIAL` constant defines if to output logged strings via serial.
 *
 */
#define CONFIG_LOG_SERIAL true

/**
 * @brief Maximum note queue configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_NOTES
 *
 * The `CONFIG_MAX_NOTES` constant defines the maximum length of a threads note queue.
 *
 */
#define CONFIG_MAX_NOTES 8

/**
 * @brief Maximum argument vector configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_ARGC
 *
 * The `CONFIG_MAX_ARGC` constant defines the maximum amount of arguments that can be passed to a process via its
 * argument vector. Used to avoid vulnerabilities where extremely large argument vectors are passed to processes.
 *
 */
#define CONFIG_MAX_ARGC 512

/**
 * @brief Minimum timer timeout configuration.
 * @ingroup kernel
 * @def CONFIG_MIN_TIMER_TIMEOUT
 *
 * The `CONFIG_MIN_TIMER_TIMEOUT` constant defines the minimum timeout that can be set for timers.
 *
 */
#define CONFIG_MIN_TIMER_TIMEOUT ((CLOCKS_PER_SEC) / 10000)

/**
 * @brief Time slice configuration.
 * @ingroup kernel
 * @def CONFIG_TIME_SLICE
 *
 * The `CONFIG_TIME_SLICE` constant defines the default time slice given to threads when they are scheduled.
 *
 */
#define CONFIG_TIME_SLICE ((CLOCKS_PER_SEC / 1000) * 10)

/**
 * @brief Cache hot threshold configuration.
 * @ingroup kernel
 * @def CONFIG_CACHE_HOT_THRESHOLD
 *
 * The `CONFIG_CACHE_HOT_THRESHOLD` constant defines the threshold below which a time duration is considered "cache
 * hot", meaning that the data is likely still in the CPU cache.
 *
 */
#define CONFIG_CACHE_HOT_THRESHOLD ((CLOCKS_PER_SEC / 1000) * 5)

/**
 * @brief Maximum mutex slow spin configuration.
 * @ingroup kernel
 * @def CONFIG_MUTEX_MAX_SLOW_SPIN
 *
 * The `CONFIG_MUTEX_MAX_SLOW_SPIN` constant defines the maximum number of iterations a thread will spin before blocking
 * on a mutex.
 *
 */
#define CONFIG_MUTEX_MAX_SLOW_SPIN 1000

/**
 * @brief Maximum screen lines configuration.
 * @ingroup kernel
 * @def CONFIG_SCREEN_MAX_LINES
 *
 * The `CONFIG_SCREEN_MAX_LINES` constant defines the maximum number of lines that the logging system will display.
 *
 */
#define CONFIG_SCREEN_MAX_LINES 256

/**
 * @brief Maximum bitmap allocator address.
 * @ingroup kernel
 * @def CONFIG_PMM_BITMAP_MAX_ADDR
 *
 * The `CONFIG_PMM_BITMAP_MAX_ADDR` constant defines the maximum address below which pages will be handled by the bitmap
 * allocator, pages above this value will be handled by the free stack allocator.
 *
 */
#define CONFIG_PMM_BITMAP_MAX_ADDR 0x4000000ULL

/**
 * @brief Process reaper interval configuration.
 * @ingroup kernel
 * @def CONFIG_PROCESS_REAPER_INTERVAL
 *
 * The `CONFIG_PROCESS_REAPER_INTERVAL` constant defines the minimum interval at which the process reaper runs to clean
 * up zombie processes. It might run less frequently.
 *
 */
#define CONFIG_PROCESS_REAPER_INTERVAL (CLOCKS_PER_SEC * 1)

/**
 * @brief Maximum environment variables configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_ENV_VARS
 *
 * The `CONFIG_MAX_ENV_VARS` constant defines the maximum number of environment variables that a process can have.
 *
 */
#define CONFIG_MAX_ENV_VARS 4096

/**
 * @brief Kernel log buffer size configuration.
 * @ingroup kernel
 * @def CONFIG_LOG_KLOG_BUFFER_SIZE
 *
 * The `CONFIG_LOG_KLOG_BUFFER_SIZE` constant defines the size of the buffer used for the `/dev/klog` file.
 *
 */
#define CONFIG_KLOG_SIZE 0x4000

/**
 * @brief Per-CPU data size configuration.
 * @ingroup kernel
 * @def CONFIG_PERCPU_SIZE
 *
 * The `CONFIG_PERCPU_SIZE` constant defines the size allocated for per-CPU data.
 *
 */
#define CONFIG_PERCPU_SIZE 0x10000