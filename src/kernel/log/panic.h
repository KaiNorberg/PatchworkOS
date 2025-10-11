#pragma once

#include "cpu/trap.h"

#include <boot/boot_info.h>
#include <common/defs.h>
#include <sys/list.h>

/**
 * @brief Panic handling
 * @defgroup kernel_log_panic Panic
 * @ingroup kernel_log
 *
 * @{
 */

/**
 * @brief Cpu ID indicating no CPU has panicked yet.
 */
#define PANIC_NO_CPU_ID UINT32_MAX

/**
 * @brief Maximum stack frames to capture in a panic.
 */
#define PANIC_MAX_STACK_FRAMES 16

/**
 * @brief Panic symbol structure.
 *
 * Stores information about a symbol for panic stack traces.
 */
typedef struct panic_symbol
{
    list_entry_t entry;
    uintptr_t start;
    uintptr_t end;
    char name[MAX_NAME];
} panic_symbol_t;

/**
 * @brief Initialize panic symbols from the bootloader-provided kernel information.
 *
 * @param kernel Pointer to the bootloader-provided kernel information.
 */
void panic_symbols_init(const boot_kernel_t* kernel);

/**
 * @brief Panic the kernel, printing a message and halting.
 */
NORETURN void panic(const trap_frame_t* trapFrame, const char* format, ...);

/** @} */
