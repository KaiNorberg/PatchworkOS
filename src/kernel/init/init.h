#pragma once

#include "cpu/cpu.h"

#include <boot/boot_info.h>

/**
 * @brief Initialization and `kmain()`.
 * @defgroup init Initialization
 * @ingroup kernel
 *
 * @{
 */

/**
 * @brief Early kernel initialization.
 *
 * This will do the absolute minimum to get the scheduler and smp up and running. Note that having smp "running" does
 * not mean the other cpus are started, just that we are keeping track of the currently active cpu.
 *
 * Having the scheduler running then lets us load the boot thread which will jump to `kmain()` where we can do the rest
 * of the kernel initialization. This thread will eventually become the idle thread of the bootstrap cpu.
 *
 * Swapping to the boot thread requires some assembly trickery which is why we need the `start.s` file.
 *
 * Will be called in the `_start()` function found in `start.s` with interrupts disabled.
 *
 * @param bootInfo Information provided by the bootloader.
 */
void init_early(const boot_info_t* info);

/**
 * @brief Initialize other CPUs.
 *
 * This will be called on the other CPUs to initalize their cpu specific stuff.
 */
void init_other_cpu(cpuid_t id);

/**
 * @brief Kernel main function.
 *
 * This is the entry point for the boot thread. After `init_early()` the `_start()` function will load the boot thread
 * causing execution to jump to this function. We can then perform the rest of the kernel initialization here and start
 * the init process.
 *
 * Check `thread_get_boot()` for the boot threads initial interrupt frame.
 *
 * Note that this function will never return, instead it will call `sched_done_with_boot_thread()` which will turn the
 * boot thread into the idle thread of the bootstrap cpu and enable interrupts.
 */
_NORETURN void kmain(void);

/** @} */
