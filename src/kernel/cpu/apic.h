#pragma once

#include "defs.h"

/**
 * @brief Apic.
 * @ingroup kernel_cpu
 *
 *
 */

#define APIC_TIMER_MASKED 0x10000
#define APIC_TIMER_PERIODIC 0x20000
#define APIC_TIMER_ONE_SHOT 0x00000

#define LAPIC_MSR_ENABLE 0x800

#define LAPIC_REG_ID 0x020
#define LAPIC_REG_EOI 0x0B0
#define LAPIC_REG_SPURIOUS 0x0F0
#define LAPIC_REG_ICR0 0x300
#define LAPIC_REG_ICR1 0x310

#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_TIMER_INITIAL_COUNT 0x380
#define LAPIC_REG_TIMER_CURRENT_COUNT 0x390
#define LAPIC_REG_TIMER_DIVIDER 0x3E0

#define LAPIC_ID_OFFSET 24

#define APIC_TIMER_DIV_16 0x3
#define APIC_TIMER_DIV_32 0x4
#define APIC_TIMER_DIV_64 0x5
#define APIC_TIMER_DIV_128 0x6

#define APIC_TIMER_DEFAULT_DIV APIC_TIMER_DIV_16

#define LAPIC_SPURIOUS_ENABLE (1 << 8)

#define APIC_TIMER_TICKS_FIXED_POINT_OFFSET 32

void apic_init(void);

void apic_timer_one_shot(uint8_t vector, uint32_t ticks);

/**
 * @brief Apic timer ticks per nanosecond.
 * @ingroup kernel_cpu
 *
 * The `apic_timer_ticks_per_ns()` function retrieves the ticks that occur every nanosecond in the apic timer on the
 * caller cpu. Due to the fact that this amount of ticks is very small, most likely less then 0, we used fixed point
 * arithmetic to store the result, the offset used for this is `APIC_TIMER_TICKS_FIXED_POINT_OFFSET`.
 *
 * @return uint64_t The number of ticks per nanosecond, stored using fixed point arithmetic.
 */
uint64_t apic_timer_ticks_per_ns(void);

void lapic_cpu_init(void);

uint8_t lapic_id(void);

void lapic_write(uint32_t reg, uint32_t value);

uint32_t lapic_read(uint32_t reg);

void lapic_send_init(uint32_t apicId);

void lapic_send_sipi(uint32_t apicId, uint32_t page);

void lapic_send_ipi(uint32_t apicId, uint8_t vector);

void lapic_eoi(void);
