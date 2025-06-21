#pragma once

#include <boot/boot_info.h>

#include "cpu/smp.h"
#include "cpu/trap.h"
#include "defs.h"

/**
 * @brief Kernel initialization.
 * @ingroup kernel
 *
 * The `kernel_init()` function calls all the needed initialization functions to fully initialize both the kernel and
 * the calling cpu, note that the `kernel_init()` function should only be called once and only on the bootstrap cpu.
 *
 * @param bootInfo The boot information passed from the bootloader.
 */
void kernel_init(boot_info_t* bootInfo);

/**
 * @brief Kernel other cpu initialization.
 * @ingroup kernel
 *
 * The `kernel_other_init()` function calls all the needed initialization functions to initialize the calling
 * cpu, should not be called on the bootstrap cpu.
 *
 */
void kernel_other_init(void);