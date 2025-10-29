#pragma once

#include <kernel/cpu/interrupt.h>

#include <stdbool.h>

/**
 * @brief Interrupt Request handling
 * @defgroup kernel_cpu_irq IRQ
 * @ingroup kernel_cpu
 *
 * @{
 */

/**
 * @brief IRQ numbers.
 * @enum irq_t
 *
 * These all start from `EXTERNAL_INTERRUPT_BASE`.
 *
 */
typedef enum
{
    IRQ_PIT = 0x0,
    IRQ_PS2_FIRST_DEVICE = 0x1,
    IRQ_CASCADE = 0x2,
    IRQ_COM2 = 0x3,
    IRQ_COM1 = 0x4,
    IRQ_LPT2 = 0x5,
    IRQ_FLOPPY = 0x6,
    IRQ_LPT1 = 0x7,
    IRQ_CMOS = 0x8,
    IRQ_FREE1 = 0x9,
    IRQ_FREE2 = 0xA,
    IRQ_FREE3 = 0xB,
    IRQ_PS2_SECOND_DEVICE = 0xC,
    IRQ_FPU = 0xD,
    IRQ_PRIMARY_ATA_HARD_DRIVE = 0xE,
    IRQ_SECONDARY_ATA_HARD_DRIVE = 0xF,
    IRQ_AMOUNT = 0x10
} irq_t;

/**
 * @brief Maximum amount of callbacks per IRQ.
 */
#define IRQ_MAX_CALLBACK 16

/**
 * @brief Callback function type for IRQs.
 */
typedef void (*irq_callback_func_t)(irq_t irq, void* data);

/**
 * @brief Structure to hold an IRQ callback and its data.
 */
typedef struct
{
    irq_callback_func_t func;
    void* data;
} irq_callback_t;

/**
 * @brief Structure to hold all callbacks for an IRQ.
 */
typedef struct
{
    irq_callback_t callbacks[IRQ_MAX_CALLBACK];
    uint32_t callbackAmount;
    bool redirected;
} irq_handler_t;

/**
 * @brief Dispatch an IRQ.
 *
 * This function is called from `interrupt_handler()` when an IRQ is received. It will call all registered callbacks
 * for the IRQ.
 *
 * @param frame The interrupt frame of the IRQ.
 */
void irq_dispatch(interrupt_frame_t* frame);

/**
 * @brief Install an IRQ handler.
 *
 * Installs the provided callback function to be called when the specified IRQ is received. The `data` pointer will be
 * passed to the callback when it is called.
 *
 * If the interrupt has not yet been redirected the ioapic will be set to redirect the vector.
 *
 * @param irq The IRQ number to install the handler for.
 * @param func The callback function to call when the IRQ is received.
 * @param data The data pointer to pass to the callback function.
 */
void irq_install(irq_t irq, irq_callback_func_t func, void* data);

/**
 * @brief Uninstall an IRQ handler.
 *
 * Uninstalls the provided callback function from the specified IRQ. If the function is not found, nothing happens.
 *
 * @param irq The IRQ number to uninstall the handler from.
 * @param func The callback function to uninstall.
 */
void irq_uninstall(irq_t irq, irq_callback_func_t func);

/** @} */
