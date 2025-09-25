#pragma once

#include <common/defs.h>

#include <stdint.h>

typedef struct cpu cpu_t;

/**
 * @brief Advanced Programmable Interrupt Controller.
 * @ingroup kernel_drivers
 * @defgroup kernel_drivers_apic APIC
 */

#define APIC_TIMER_MASKED 0x10000
#define APIC_TIMER_PERIODIC 0x20000
#define APIC_TIMER_ONE_SHOT 0x00000

#define APIC_TIMER_DIV_16 0x3
#define APIC_TIMER_DIV_32 0x4
#define APIC_TIMER_DIV_64 0x5
#define APIC_TIMER_DIV_128 0x6

#define APIC_TIMER_DEFAULT_DIV APIC_TIMER_DIV_16

#define LAPIC_MSR_ENABLE 0x800
#define LAPIC_MSR_BSP 0x100

#define LAPIC_REG_ID 0x020
#define LAPIC_REG_VERSION 0x030
#define LAPIC_REG_TASK_PRIORITY 0x080
#define LAPIC_REG_ARBITRATION_PRIORITY 0x090
#define LAPIC_REG_PROCESSOR_PRIORITY 0x0A0
#define LAPIC_REG_EOI 0x0B0
#define LAPIC_REG_REMOTE_READ 0x0C0
#define LAPIC_REG_LOGICAL_DEST 0x0D0
#define LAPIC_REG_DEST_FORMAT 0x0E0
#define LAPIC_REG_SPURIOUS 0x0F0
#define LAPIC_REG_ISR_BASE 0x100
#define LAPIC_REG_TMR_BASE 0x180
#define LAPIC_REG_IRR_BASE 0x200
#define LAPIC_REG_ERROR_STATUS 0x280
#define LAPIC_REG_LVT_CMCI 0x2F0
#define LAPIC_REG_ICR0 0x300
#define LAPIC_REG_ICR1 0x310
#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_LVT_THERMAL 0x330
#define LAPIC_REG_LVT_PERFCTR 0x340
#define LAPIC_REG_LVT_LINT0 0x350
#define LAPIC_REG_LVT_LINT1 0x360
#define LAPIC_REG_LVT_ERROR 0x370
#define LAPIC_REG_TIMER_INITIAL_COUNT 0x380
#define LAPIC_REG_TIMER_CURRENT_COUNT 0x390
#define LAPIC_REG_TIMER_DIVIDER 0x3E0

#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_TIMER_INITIAL_COUNT 0x380
#define LAPIC_REG_TIMER_CURRENT_COUNT 0x390
#define LAPIC_REG_TIMER_DIVIDER 0x3E0

#define LAPIC_ID_OFFSET 24
#define LAPIC_SPURIOUS_ENABLE (1 << 8)
#define LAPIC_LVT_MASKED (1 << 16)

#define LAPIC_ICR_FIXED (0 << 8)
#define LAPIC_ICR_LOWEST_PRIORITY (1 << 8)
#define LAPIC_ICR_SMI (2 << 8)
#define LAPIC_ICR_NMI (4 << 8)
#define LAPIC_ICR_INIT (5 << 8)
#define LAPIC_ICR_STARTUP (6 << 8)
#define LAPIC_ICR_CLEAR_INIT_LEVEL (1 << 14)

#define IOAPIC_REG_SELECT 0x00
#define IOAPIC_REG_DATA 0x10

#define IOAPIC_REG_VERSION 0x01
#define IOAPIC_REG_REDIRECTION(pin, high) (0x10 + (pin) * 2 + (high))

#define APIC_TIMER_TICKS_FIXED_POINT_OFFSET 32

typedef struct PACKED
{
    union {
        uint32_t raw;
        struct PACKED
        {
            uint8_t version;
            uint8_t reserved;
            uint8_t maxRedirs;
            uint8_t reserved2;
        };
    };
} ioapic_version_t;

typedef enum
{
    IOAPIC_DELIVERY_NORMAL = 0,
    IOAPIC_DELIVERY_LOW_PRIO = 1,
    IOAPIC_DELIVERY_SMI = 2,
    IOAPIC_DELIVERY_NMI = 4,
    IOAPIC_DELIVERY_INIT = 5,
    IOAPIC_DELIVERY_EXTERNAL = 7
} ioapic_delivery_mode_t;

typedef enum
{
    IOAPIC_DESTINATION_PHYSICAL = 0,
    IOAPIC_DESTINATION_LOGICAL = 1
} ioapic_destination_mode_t;

typedef enum
{
    IOAPIC_TRIGGER_EDGE = 0,
    IOAPIC_TRIGGER_LEVEL = 1
} ioapic_trigger_mode_t;

typedef enum
{
    IOAPIC_POLARITY_HIGH = 0,
    IOAPIC_POLARITY_LOW = 1
} ioapic_polarity_t;

typedef union {
    struct PACKED
    {
        uint8_t vector;
        ioapic_delivery_mode_t deliveryMode : 3;
        ioapic_destination_mode_t destinationMode : 1;
        uint8_t deliveryStatus : 1;
        ioapic_polarity_t polarity : 1;
        uint8_t remoteIRR : 1;
        ioapic_trigger_mode_t triggerMode : 1;
        uint8_t mask : 1;
        uint64_t reserved : 39;
        uint8_t destination : 8;
    };
    struct PACKED
    {
        uint32_t low;
        uint32_t high;
    } raw;
} ioapic_redirect_entry_t;

typedef struct
{
    uintptr_t base;
    uint32_t gsiBase;
    uint32_t maxRedirs;
} ioapic_t;

typedef uint32_t ioapic_id_t;
typedef uint32_t lapic_id_t;

void apic_timer_one_shot(uint8_t vector, uint32_t ticks);

/**
 * @brief Apic timer ticks per nanosecond.
 * @ingroup kernel_drivers_apic
 *
 * The `apic_timer_ticks_per_ns()` function retrieves the ticks that occur every nanosecond in the apic timer on the
 * caller cpu. Due to the fact that this amount of ticks is very small, most likely less then 1, we used fixed point
 * arithmetic to store the result, the offset used for this is `APIC_TIMER_TICKS_FIXED_POINT_OFFSET`.
 *
 * @return The number of ticks per nanosecond, stored using fixed point arithmetic.
 */
uint64_t apic_timer_ticks_per_ns(void);

void lapic_init(void);

void lapic_cpu_init(void);

uint8_t lapic_self_id(void);

void lapic_write(uint32_t reg, uint32_t value);

uint32_t lapic_read(uint32_t reg);

void lapic_send_init(uint32_t apicId);

void lapic_send_sipi(uint32_t apicId, void* entryPoint);

void lapic_send_ipi(uint32_t apicId, uint8_t vector);

void lapic_eoi(void);

void ioapic_all_init(void);

void ioapic_set_redirect(uint8_t vector, uint32_t gsi, ioapic_delivery_mode_t deliveryMode, ioapic_polarity_t polarity,
    ioapic_trigger_mode_t triggerMode, cpu_t* cpu, bool enable);
