#pragma once

/**
 * @brief Time slice configuration.
 * @ingroup kernel
 *
 * The `CONFIG_TIME_SLICE` constant defines the length of time, in clock cycles, that each thread is allowed to run
 * before another is forcibly scheduled.
 *
 */
#define CONFIG_TIME_SLICE (CLOCKS_PER_SEC / 100)

/**
 * @brief Timer frequency configuration.
 * @ingroup kernel
 *
 * The `CONFIG_TIMER_HZ` constant defines the amount of times per second the `VECTOR_TIMER` trap will occur on each cpu.
 *
 */
#define CONFIG_TIMER_HZ 1024

/**
 * @brief Kernel stack configuration.
 * @ingroup kernel.
 *
 * The `CONFIG_KERNEL_STACK` constant defines the size of the kernel stack for each thread, each thread has their own
 * kernel stack which is switched to either when processing a trap or when invoking a system call, on short whenever the
 * thread jumps to kernel space.
 *
 */
#define CONFIG_KERNEL_STACK 0x1000

/**
 * @brief User stack configuration.
 * @ingroup kernel
 *
 * The `CONFIG_MAX_USER_STACK_PAGES` constant defines the maximum amount of pages that are allowed to be allocated for a
 * threads user stack, the user stack is used while the thread is in user space.
 *
 */
#define CONFIG_MAX_USER_STACK_PAGES 2000

/**
 * @brief Maximum file descriptor configuration.
 * @ingroup kernel
 *
 * The `CONFIG_MAX_FD` constant defines the maximum amount of file descriptors that a process is allowed to have open.
 *
 */
#define CONFIG_MAX_FD 64

/**
 * @brief Serial logging configuration.
 * @ingroup kernel
 *
 * The `CONFIG_LOG_SERIAL` constant defines if to output logged strings via serial.
 *
 */
#define CONFIG_LOG_SERIAL true

/**
 * @brief Maximum note queue configuration.
 * @ingroup kernel
 *
 * The `CONFIG_MAX_NOTES` constant defines the maximum length of a threads note queue. If a thread is unable to receive
 * the notes in time before the queue fills up, then notes will be discarded, unless they are flagged as NOTE_CRITICAL.
 *
 */
#define CONFIG_MAX_NOTES 8
