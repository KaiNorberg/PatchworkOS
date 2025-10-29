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

/**
 * @brief Page Fault Error Codes.
 * @enum page_fault_errors_t
 *
 * Will be stored in the error code of the interrupt frame on a page fault.
 */
typedef enum
{
    PAGE_FAULT_PRESENT = 1 << 0,
    PAGE_FAULT_WRITE = 1 << 1,
    PAGE_FAULT_USER = 1 << 2,
    PAGE_FAULT_RESERVED = 1 << 3,
    PAGE_FAULT_INSTRUCTION = 1 << 4,
    PAGE_FAULT_PROTECTION_KEY = 1 << 5,
    PAGE_FAULT_SHADOW_STACK = 1 << 6,
    PAGE_FAULT_SOFTWARE_GUARD_EXT = 1 << 7,
} page_fault_errors_t;

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
 * @brief CPU vector identifiers.
 *
 * External interrupts (IRQ's) are defined in `irq_t`.
 */
typedef enum
{
    EXCEPTION_DIVIDE_ERROR = 0x0,
    EXCEPTION_DEBUG = 0x1,
    EXCEPTION_NMI = 0x2,
    EXCEPTION_BREAKPOINT = 0x3,
    EXCEPTION_OVERFLOW = 0x4,
    EXCEPTION_BOUND_RANGE_EXCEEDED = 0x5,
    EXCEPTION_INVALID_OPCODE = 0x6,
    EXCEPTION_DEVICE_NOT_AVAILABLE = 0x7,
    EXCEPTION_DOUBLE_FAULT = 0x8,
    EXCEPTION_COPROCESSOR_SEGMENT_OVERRUN = 0x9,
    EXCEPTION_INVALID_TSS = 0xA,
    EXCEPTION_SEGMENT_NOT_PRESENT = 0xB,
    EXCEPTION_STACK_FAULT = 0xC,
    EXCEPTION_GENERAL_PROTECTION = 0xD,
    EXCEPTION_PAGE_FAULT = 0xE,
    EXCEPTION_RESERVED = 0xF,
    EXCEPTION_X87_FPU_ERROR = 0x10,
    EXCEPTION_ALIGNMENT_CHECK = 0x11,
    EXCEPTION_MACHINE_CHECK = 0x12,
    EXCEPTION_SIMD_EXCEPTION = 0x13,
    EXCEPTION_VIRTUALIZATION_EXCEPTION = 0x14,
    EXCEPTION_CONTROL_PROTECTION_EXCEPTION = 0x15,
    EXCEPTION_RESERVED_16 = 0x16,
    EXCEPTION_RESERVED_17 = 0x17,
    EXCEPTION_RESERVED_18 = 0x18,
    EXCEPTION_RESERVED_19 = 0x19,
    EXCEPTION_RESERVED_1A = 0x1A,
    EXCEPTION_RESERVED_1B = 0x1B,
    EXCEPTION_RESERVED_1C = 0x1C,
    EXCEPTION_RESERVED_1D = 0x1D,
    EXCEPTION_RESERVED_1E = 0x1E,
    EXCEPTION_RESERVED_1F = 0x1F,
    EXCEPTION_AMOUNT = 0x20,

    EXTERNAL_INTERRUPT_BASE = 0x20,

    INTERRUPT_TLB_SHOOTDOWN = 0xFA, ///< TLB shootdown interrupt.
    INTERRUPT_DIE = 0xFB,           ///< Kills and frees the current thread.
    INTERRUPT_NOTE = 0xFC,          ///< Nofify that a note is available.
    INTERRUPT_TIMER = 0xFD,         ///< The timer subsystem interrupt.
    INTERRUPT_HALT = 0xFE,          ///< Halt the CPU.
    INTERRUPT_AMOUNT = 0xFF
} interrupt_t;

/**
 * @brief Pointers to functions to handle each vector.
 */
extern void* vectorTable[INTERRUPT_AMOUNT];

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
