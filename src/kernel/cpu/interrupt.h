#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Interrupt Handling
 * @defgroup kernel_cpu_interrupt Interrupts
 * @ingroup kernel_cpu
 *
 * This module provides structures and functions for handling CPU interrupts.
 *
 * @{
 */

#define PAGE_FAULT_PRESENT (1 << 0)

/**
 * @brief Trap Frame Structure.
 * @struct interrupt_frame_t
 *
 * Stores the CPU state at the time of a interrupt, usefull for context switching as we can modify the
 * registers before returning from the interrupt.
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
} interrupt_frame_t;

/**
 * @brief Checks if a interrupt frame is from user space.
 * @def INTERRUPT_FRAME_IN_USER_SPACE
 *
 * @param frame The interrupt frame to check.
 * @return true if the interrupt frame is from user space, false otherwise.
 */
#define INTERRUPT_FRAME_IN_USER_SPACE(frame) ((frame)->ss == (GDT_SS_RING3) && (frame)->cs == (GDT_CS_RING3))

/**
 * @brief Per-CPU Interrupt Context.
 * @struct interrupt_ctx_t
 *
 * Used to manage nested CLI (Clear Interrupt Flag) calls and track interrupt depth.
 */
typedef struct
{
    uint64_t oldRflags;
    uint32_t disableDepth;
    bool inInterrupt;
} interrupt_ctx_t;

/**
 * @brief Initializes the CLI context.
 *
 * @param cli The CLI context to initialize.
 */
void interrupt_ctx_init(interrupt_ctx_t* ctx);

/**
 * @brief Disable interrupts and increment the disableDepth.
 *
 * Must have a matching `interrupt_enable()` call to re-enable interrupts when depth reaches zero.
 */
void interrupt_disable(void);

/**
 * @brief Decrement the CLI depth and enable interrupts if depth reaches zero and interrupts were previously enabled.
 */
void interrupt_enable(void);

/**
 * @brief Handles CPU interrupts.
 *
 * This will be called from `vector_common` in `vectors.s`.
 *
 * @param frame The interrupt frame containing the CPU state at the time of the exception.
 */
void interrupt_handler(interrupt_frame_t* frame);

/** @} */
