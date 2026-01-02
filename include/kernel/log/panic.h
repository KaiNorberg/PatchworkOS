#pragma once

#include <sys/defs.h>

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
 * @brief QEMU exit port for panic.
 */
#define QEMU_EXIT_ON_PANIC_PORT 0x501

/**
 * @brief Print a stack trace from a interrupt frame.
 *
 * Will NOT panic the kernel, just print the stack trace.
 *
 * @param frame Pointer to the interrupt frame.
 */
void panic_stack_trace(const interrupt_frame_t* frame);

/**
 * @brief Panic the kernel, printing a message and halting.
 *
 * If `QEMU_EXIT_ON_PANIC` is defined and we are running in QEMU, will exit QEMU instead of halting.
 *
 * @param frame Pointer to the interrupt frame, can be `NULL`.
 * @param format The format string for the panic message.
 * @param ... Additional arguments for the format string.
 */
NORETURN void panic(const interrupt_frame_t* frame, const char* format, ...);

/** @} */
