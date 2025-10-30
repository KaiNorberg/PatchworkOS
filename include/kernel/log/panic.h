#pragma once

#include <kernel/defs.h>

#include <boot/boot_info.h>

#include <sys/list.h>

typedef struct interrupt_frame interrupt_frame_t;
typedef struct cpu cpu_t;

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
 * @brief Print a stack trace from a interrupt frame.
 *
 * Will NOT panic the kernel, just print the stack trace.
 *
 * @param frame Pointer to the interrupt frame.
 */
void panic_stack_trace(const interrupt_frame_t* frame);

/**
 * @brief Initialize panic symbols from the bootloader-provided kernel information.
 *
 * @param kernel Pointer to the bootloader-provided kernel information.
 */
void panic_symbols_init(const boot_kernel_t* kernel);

/**
 * @brief Panic the kernel, printing a message and halting.
 *
 * @param frame Pointer to the interrupt frame, can be `NULL`.
 * @param format The format string for the panic message.
 * @param ... Additional arguments for the format string.
 */
NORETURN void panic(const interrupt_frame_t* frame, const char* format, ...);

/** @} */
