#pragma once

#include <stdint.h>

/**
 * @brief Trap and Interrupt Handling
 * @defgroup kernel_cpu_trap Trap
 * @ingroup kernel_cpu
 *
 * This module provides structures and functions for handling CPU traps and interrupts.
 *
 * @{
 */

#define PAGE_FAULT_PRESENT (1 << 0)

/**
 * @brief Trap Frame Structure.
 * @struct trap_frame_t
 *
 * Stores the CPU state at the time of a trap or interrupt, usefull for context switching as we can modify the
 * registers before returning from the trap/interrupt.
 */
typedef struct PACKED
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t vector;
    uint64_t errorCode;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} trap_frame_t;

/**
 * @brief Checks if a trap frame is from user space.
 * @def TRAP_FRAME_IN_USER_SPACE
 *
 * @param trapFrame The trap frame to check.
 * @return true if the trap frame is from user space, false otherwise.
 */
#define TRAP_FRAME_IN_USER_SPACE(trapFrame) ((trapFrame)->ss == (GDT_SS_RING3) && (trapFrame)->cs == (GDT_CS_RING3))

/**
 * @brief Per-CPU CLI Context.
 * @struct cli_ctx_t
 *
 * Used to manage nested CLI (Clear Interrupt Flag) calls.
 */
typedef struct
{
    uint64_t oldRflags;
    uint64_t depth;
} cli_ctx_t;

/**
 * @brief Initializes the CLI context.
 *
 * @param cli The CLI context to initialize.
 */
void cli_ctx_init(cli_ctx_t* cli);

/**
 * @brief Disable interrupts and increment the CLI depth.
 *
 * Must have a matching `cli_pop()` call to re-enable interrupts.
 */
void cli_push(void);

/**
 * @brief Decrement the CLI depth and enable interrupts if depth reaches zero and interrupts were previously enabled.
 */
void cli_pop(void);

/**
 * @brief Handles CPU interrupts.
 *
 * This will be called from `vector_common` in `vectors.s`.
 *
 * @param trapFrame The trap frame containing the CPU state at the time of the exception.
 */
void trap_handler(trap_frame_t* trapFrame);

/** @} */
