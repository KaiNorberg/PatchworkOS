#pragma once

#include <boot/boot_info.h>

/**
 * @brief Kernel initialization.
 * @defgroup kernel_init Kernel Initialization
 *
 * @{
 */

/**
 * @brief Early kernel initialization.
 *
 * This will do the absolute minimum to get the scheduler and smp up and running. Note that having smp "running" does
 * not mean the other cpus are started, just that we are keeping track of the currently active cpu.
 *
 * Having the scheduler running then lets us start using the kernel stack of the boot thread, which is just the first
 * thread created in the kernel process. This thread will eventually become the idle thread of the bootstrap cpu.
 *
 * Swapping to the boot thread requires some assembly trickery which is why we need the `start.s` file.
 *
 * Will be called in `_start` found in `start.s`.
 *
 * @param bootInfo Information provided by the bootloader.
 */
void kernel_early_init(const boot_info_t* bootInfo);

/**
 * @brief Finish kernel initialization.
 *
 * This will finish the rest of the kernel initialization and will be called after `kernel_early_init()` while running
 * on the boot thread from `_start` found in `start.s`.
 *
 * Will not spawn the init process, that will be done in `kmain()`.
 *
 * @param bootInfo Information provided by the bootloader.
 */
void kernel_init(const boot_info_t* bootInfo);

/**
 * @brief Initialize other CPUs.
 *
 * This will be called on the other CPUs to initalize their cpu specific stuff.
 */
void kernel_other_cpu_init(void);

/**
 * @brief Kernel main function.
 *
 * This will be called after `kernel_init()` while running on the boot thread from `_start` found in `start.s`.
 *
 * This will spawn the init process, which is the first user process.
 *
 * Note that this function will never return, instead it will call `sched_done_with_boot_thread()` which will turn the
 * boot thread into the idle thread of the bootstrap cpu and enable interrupts.
 */
_NORETURN void kmain(void);

/** @} */
