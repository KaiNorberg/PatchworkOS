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

#define CONFIG_IDLE_TIME_SLICE ((CLOCKS_PER_SEC / 100))

#define CONFIG_MAX_TIME_SLICE ((CLOCKS_PER_SEC / 1000) * 500)

#define CONFIG_MIN_TIME_SLICE ((CLOCKS_PER_SEC / 1000) * 5)

#define CONFIG_MAX_RECENT_BLOCK_TIME ((CLOCKS_PER_SEC / 1000) * 10)

#define CONFIG_MAX_PRIORITY_BOOST 5

#define CONFIG_MAX_PRIORITY_PENALTY 5

#define CONFIG_LOAD_BALANCE_BIAS 2