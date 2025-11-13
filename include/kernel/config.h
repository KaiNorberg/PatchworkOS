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
 * @brief CPU data configuration.
 * @ingroup kernel
 * @def CONFIG_CPU_DATA_PAGES
 *
 * The `CONFIG_CPU_DATA_PAGES` constant defines the amount of pages allocated for per-CPU data.
 *
 */
#define CONFIG_CPU_DATA_PAGES 16

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
 * The `CONFIG_MAX_NOTES` constant defines the maximum length of a threads note queue. If a thread is unable to receive
 * the notes in time before the queue fills up, then notes will be discarded, unless they are flagged as NOTE_CRITICAL.
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
 * @brief Maximum time slice configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_TIME_SLICE
 *
 * The `CONFIG_MAX_TIME_SLICE` constant defines the maximum time slice a thread can have based on its priority.
 *
 */
#define CONFIG_MAX_TIME_SLICE ((CLOCKS_PER_SEC / 1000) * 50)

/**
 * @brief Minimum time slice configuration.
 * @ingroup kernel
 * @def CONFIG_MIN_TIME_SLICE
 *
 * The `CONFIG_MIN_TIME_SLICE` constant defines the minimum time slice a thread can have based on its priority.
 *
 */
#define CONFIG_MIN_TIME_SLICE ((CLOCKS_PER_SEC / 1000) * 1)

/**
 * @brief Maximum recent block time configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_RECENT_BLOCK_TIME
 *
 * The `CONFIG_MAX_RECENT_BLOCK_TIME` constant defines the length of time considered when deciding if a thread is I/O or
 * CPU bound.
 *
 */
#define CONFIG_MAX_RECENT_BLOCK_TIME ((CLOCKS_PER_SEC / 1000) * 10)

/**
 * @brief Maximum priority boost configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_PRIORITY_BOOST
 *
 * The `CONFIG_MAX_PRIORITY_BOOST` constant defines the maximum priority boost a thread can receive from being I/O
 * bound.
 *
 */
#define CONFIG_MAX_PRIORITY_BOOST 8

/**
 * @brief Maximum priority penalty configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_PRIORITY_PENALTY
 *
 * The `CONFIG_MAX_PRIORITY_PENALTY` constant defines the maximum priority penalty a thread can receive from being CPU
 * bound.
 *
 */
#define CONFIG_MAX_PRIORITY_PENALTY 8

/**
 * @brief Load balance bias configuration.
 * @ingroup kernel
 * @def CONFIG_LOAD_BALANCE_BIAS
 *
 * The `CONFIG_LOAD_BALANCE_BIAS` constant defines the bias used the minimum inbalance required for load balancing to
 * occur.
 *
 */
#define CONFIG_LOAD_BALANCE_BIAS 2

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
