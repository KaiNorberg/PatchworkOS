#pragma once

#include "vectors.h"

#include <common/defs.h>

#include <stdint.h>

/**
 * @brief Interrupt Descriptor Table
 * @defgroup kernel_cpu_idt IDT
 * @ingroup kernel_cpu
 *
 * @{
 */

#define IDT_INTERRUPT_GATE 0b1110
#define IDT_TRAP_GATE 0b1111

#define IDT_RING0 0b00
#define IDT_RING1 0b01
#define IDT_RING2 0b10
#define IDT_RING3 0b11

typedef struct PACKED
{
    uint16_t size;
    uint64_t offset;
} idt_desc_t;

typedef struct PACKED
{
    uint16_t isrLow;
    uint16_t codeSegment;
    uint8_t ist;
    uint8_t attributes;
    uint16_t isrMid;
    uint32_t isrHigh;
    uint32_t reserved;
} idt_entry_t;

typedef struct PACKED
{
    idt_entry_t entries[VECTOR_AMOUNT];
} idt_t;

void idt_cpu_init(void);

void idt_load(void);

/** @} */
