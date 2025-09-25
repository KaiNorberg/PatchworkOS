#pragma once

#include "gdt.h"

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
 * @brief Trap frame structure
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

#define TRAP_FRAME_IN_USER_SPACE(trapFrame) \
    ((trapFrame)->ss == (GDT_USER_DATA | GDT_RING3) && (trapFrame)->cs == (GDT_USER_CODE | GDT_RING3))

typedef struct
{
    uint64_t oldRflags;
    uint64_t depth;
} cli_ctx_t;

void cli_ctx_init(cli_ctx_t* cli);

void cli_push(void);

void cli_pop(void);

void trap_handler(trap_frame_t* trapFrame);

/** @} */
