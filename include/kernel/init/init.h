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
 * of the kernel initialization.
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
 * Will never return, the boot thread will exit itself when done.
 *
 * @param bootInfo Information provided by the bootloader.
 */
_NORETURN void kmain(const boot_info_t* bootInfo);

/** @} */
