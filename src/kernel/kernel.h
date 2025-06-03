#pragma once

#include <bootloader/boot_info.h>

#include "cpu/smp.h"
#include "cpu/trap.h"
#include "defs.h"

/**
 * @brief Kernel initialization.
 *
 * The `kernel_init()` function calls all the needed initialization functions to fully initialize both the kernel and
 * the calling cpu, note that the `kernel_init()` function should only be called once and only on the bootstrap cpu.
 *
 * @param bootInfo The boot information passed from the bootloader.
 */
void kernel_init(boot_info_t* bootInfo);

/**
 * @brief Kernel other cpu initialization.
 *
 * The `kernel_other_init()` function calls all the needed initialization functions needed to initialize the calling
 * cpu, should not be called on the bootstrap cpu.
 *
 */
void kernel_other_init(void);

/**
 * @brief Provides the kernel an opportunity to perform busy work.
 *
 * The `kernel_checkpoint()` function gives the kernel an opportunity
 * to perform certain work like scheduling or safety checks. This function should only be called att the end of a trap
 * or by using `kernel_checkpoint_invoke()`.
 *
 * @param trapFrame The current trap frame, may be modifed if scheduling occurs.
 * @param self The currently running cpu.
 * @return Returns true if scheduling occoured, false otherwise.
 */
bool kernel_checkpoint(trap_frame_t* trapFrame, cpu_t* self);

/**
 * @brief Wrapper around `kernel_checkpoint()`.
 *
 * The `kernel_checkpoint_invoke()` function constructs a trap frame using current cpu state and then calls
 * `kernel_checkpoint()`.
 *
 */
extern void kernel_checkpoint_invoke(void);