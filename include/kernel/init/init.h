#pragma once

#include <kernel/cpu/cpu.h>

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
 * This will do the absolute minimum to get the scheduler running.
 *
 * Having the scheduler running lets us load the boot thread which will jump to `kmain()` where we can do the rest
 * of the kernel initialization. This thread will eventually become the idle thread of the bootstrap cpu.
 *
 * Will be called in the `_start()` function found in `start.s` with interrupts disabled.
 *
 * @param bootInfo Information provided by the bootloader.
 */
_NORETURN void init_early(const boot_info_t* bootInfo);

/**
 * @brief Kernel main function.
 *
 * This is the entry point for the boot thread. When `init_early()` jumps to the boot thread we will end up here. We
 * then perform the rest of the kernel initialization here and start the init process.
 *
 * Will never return, instead it will call `sched_done_with_boot_thread()` which will turn the boot thread into the idle
 * thread of the bootstrap cpu.
 *
 * @param bootInfo Information provided by the bootloader.
 */
_NORETURN void kmain(const boot_info_t* bootInfo);

/** @} */
