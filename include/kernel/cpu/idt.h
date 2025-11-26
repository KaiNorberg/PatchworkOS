#pragma once

#include <kernel/defs.h>

#include <stdint.h>

/**
 * @brief Interrupt Descriptor Table
 * @defgroup kernel_cpu_idt IDT
 * @ingroup kernel_cpu
 *
 * The Interrupt Descriptor Table tells a CPU what to do when it receives an interrupt or exception.
 *
 * @{
 */

/**
 * @brief Number of IDT gates.
 */
#define IDT_GATE_AMOUNT (UINT8_MAX + 1)

/**
 * @brief IDT gate attributes.
 * @enum idt_attributes_t
 */
typedef enum
{
    IDT_ATTR_INTERRUPT = 0b1110, ///< Interrupt gate, will disable interrupts when invoked.
    IDT_ATTR_TRAP = 0b1111,      ///< Trap gate, will NOT disable interrupts when invoked.

    IDT_ATTR_RING0 = 0b00, ///< Can be invoked from ring 0 or hardware only.
    IDT_ATTR_RING1 = 0b01, ///< Can be invoked from ring 1 or lower.
    IDT_ATTR_RING2 = 0b10, ///< Can be invoked from ring 2 or lower.
    IDT_ATTR_RING3 = 0b11, ///< Can be invoked from ring 3 or lower.

    IDT_ATTR_PRESENT = 1 << 7 ///< Must be set for the entry to be valid.
} idt_attributes_t;

/**
 * @brief IDT descriptor structure.
 * @struct idt_desc_t
 *
 * Used to load the IDT with the `lidt` instruction.
 */
typedef struct PACKED
{
    uint16_t size;   ///< Size of the IDT in bytes - 1.
    uint64_t offset; ///< Address of the IDT.
} idt_desc_t;

/**
 * @brief IDT gate structure.
 * @struct idt_gate_t
 *
 * Represents a single entry in the IDT.
 */
typedef struct PACKED
{
    uint16_t offsetLow;   ///< Lower 16 bits of handler function address.
    uint16_t codeSegment; ///< Code segment selector in the GDT.
    uint8_t ist;          ///< Interrupt Stack Table offset, 0 = dont use IST, see `tss_t`.
    uint8_t attributes;   ///< Type and attributes, see `idt_attributes_t`.
    uint16_t offsetMid;   ///< Middle 16 bits of handler function address.
    uint32_t offsetHigh;  ///< Upper 32 bits of handler function address.
    uint32_t reserved;
} idt_gate_t;

/**
 * @brief IDT structure.
 * @struct idt_t
 *
 * Represents the entire IDT.
 */
typedef struct PACKED
{
    idt_gate_t entries[IDT_GATE_AMOUNT];
} idt_t;

/**
 * @brief Initialize the IDT structure in memory.
 *
 * This will setup the IDT structure in memory, but will not load it. Loading is done in `idt_cpu_load()`.
 *
 * The IDT is setup according to the values in `irq_virt_t`.
 */
void idt_init(void);

/**
 * @brief Load the IDT on the current CPU.
 *
 * This will load the IDT using the `lidt` instruction.
 *
 * Must be called after `idt_init()`.
 */
void idt_cpu_load(void);

/** @} */
