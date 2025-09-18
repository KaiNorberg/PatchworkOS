#pragma once

#include "smp.h"

/**
 * @brief Trampoline for CPU initialization
 * @defgroup kernel_cpu_trampoline Trampoline
 * @ingroup kernel_cpu
 *
 * The trampoline is a small piece of code used during the initialization of other CPUs in a multiprocessor system.
 *
 * The trampoline code must be position-independent and fit within a single memory page, this is why we do all the
 * weird offset stuff.
 *
 * @{
 */

#define TRAMPOLINE_BASE_ADDR 0x8000
#define TRAMPOLINE_DATA_OFFSET 0x0F00

#define TRAMPOLINE_PML4_OFFSET (TRAMPOLINE_DATA_OFFSET + 0xF0)
#define TRAMPOLINE_STACK_OFFSET (TRAMPOLINE_DATA_OFFSET + 0xE8)
#define TRAMPOLINE_ENTRY_OFFSET (TRAMPOLINE_DATA_OFFSET + 0xE0)
#define TRAMPOLINE_CPU_ID_OFFSET (TRAMPOLINE_DATA_OFFSET + 0xD8)

#define TRAMPOLINE_ADDR(offset) ((void*)(TRAMPOLINE_BASE_ADDR + (offset)))

extern void trampoline_start(void);
extern void trampoline_end(void);

#define TRAMPOLINE_SIZE ((uintptr_t)trampoline_end - (uintptr_t)trampoline_start)

void trampoline_init(void);

uint64_t trampoline_cpu_setup(cpuid_t cpuId, uint64_t stackTop, void (*entry)(cpuid_t));

uint64_t trampoline_wait_ready(cpuid_t cpuId, clock_t timeout);

void trampoline_signal_ready(cpuid_t cpuId);

void trampoline_deinit(void);

/** @} */
