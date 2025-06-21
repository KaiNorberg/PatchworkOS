#pragma once

/**
 * @brief Kernel stack configuration.
 * @ingroup kernel
 * @def CONFIG_KERNEL_STACK
 *
 * The `CONFIG_KERNEL_STACK` constant defines the size of the kernel stack for each thread, each thread has their own
 * kernel stack which is switched to either when processing a trap or when invoking a system call, on short whenever the
 * thread jumps to kernel space.
 *
 */
#define CONFIG_KERNEL_STACK 0x4000

/**
 * @brief User stack configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_USER_STACK_PAGES
 *
 * The `CONFIG_MAX_USER_STACK_PAGES` constant defines the maximum amount of pages that are allowed to be allocated for a
 * threads user stack, the user stack is used while the thread is in user space.
 *
 */
#define CONFIG_MAX_USER_STACK_PAGES 2000

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
 * The `CONFIG_MAX_RECENT_BLOCK_TIME` constant defines the length of time considered when deciding if a thread is I/O or CPU bound.
 *
 */
#define CONFIG_MAX_RECENT_BLOCK_TIME ((CLOCKS_PER_SEC / 1000) * 10)

/**
 * @brief Maximum priority boost configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_PRIORITY_BOOST
 *
 * The `CONFIG_MAX_PRIORITY_BOOST` constant defines the maximum priority boost a thread can receive from being I/O bound.
 *
 */
#define CONFIG_MAX_PRIORITY_BOOST 8

/**
 * @brief Maximum priority penalty configuration.
 * @ingroup kernel
 * @def CONFIG_MAX_PRIORITY_PENALTY
 *
 * The `CONFIG_MAX_PRIORITY_PENALTY` constant defines the maximum priority penalty a thread can receive from being CPU bound.
 *
 */
#define CONFIG_MAX_PRIORITY_PENALTY 8

/**
 * @brief Load balance bias configuration.
 * @ingroup kernel
 * @def CONFIG_LOAD_BALANCE_BIAS
 *
 * The `CONFIG_LOAD_BALANCE_BIAS` constant defines the bias used the minimum inbalance required for load balancing to occur.
 *
 */
#define CONFIG_LOAD_BALANCE_BIAS 2
