#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Interrupt Requests (IRQs)
 * @defgroup kernel_cpu_irq IRQ
 * @ingroup kernel_cpu
 *
 * For interrupt handling we use a system of IRQs, where the hardware trigger a physical IRQ (`irq_phys_t`) which is then mapped to a virtual IRQ (`irq_virt_t`) using a `irq_chip_t`. 
 * 
 * ## Physical vs Virtual IRQs
 *
 * The IRQ chips are implemented in a driver, usually it would be the IOAPIC, and they are responsible for the actual physical to virtual mapping. 
 * 
 * Note that physical to virtual mapping might not be 1:1, they can differ per CPU, and that there could be multiple `irq_chip_t`s in the system handling separate or overlapping ranges of physical IRQs.
 *
 * So, for example, say we receive a physical IRQ 1, which is usually the ps2 keyboard interrupt. Lets also say we have a single IRQ chip, the IOAPIC, which is configured to map physical IRQ 1 to virtual IRQ 0x21 on CPU 0. We would then see all callbacks registered for virtual IRQ 0x21 being called on CPU 0.
 *
 * This means that while the physical IRQs can be whatever, the virtual IRQs are static and defined by the kernel.
 * 
 * @{
 */

/**
 * @brief Physical IRQ numbers.
 * @typedef irq_phys_t
 */
typedef uint32_t irq_phys_t;

/**
 * @brief Virtual IRQ numbers.
 * @enum irq_virt_t
 */
typedef enum
{
    IRQ_VIRT_DIVIDE_ERROR = 0x0,
    IRQ_VIRT_DEBUG = 0x1,
    IRQ_VIRT_BREAKPOINT = 0x3,
    IRQ_VIRT_OVERFLOW = 0x4,
    IRQ_VIRT_BOUND_RANGE_EXCEEDED = 0x5,
    IRQ_VIRT_INVALID_OPCODE = 0x6,
    IRQ_VIRT_DEVICE_NOT_AVAILABLE = 0x7,
    IRQ_VIRT_DOUBLE_FAULT = 0x8,
    IRQ_VIRT_COPROCESSOR_SEGMENT_OVERRUN = 0x9,
    IRQ_VIRT_INVALID_TSS = 0xA,
    IRQ_VIRT_SEGMENT_NOT_PRESENT = 0xB,
    IRQ_VIRT_STACK_FAULT = 0xC,
    IRQ_VIRT_GENERAL_PROTECTION = 0xD,
    IRQ_VIRT_PAGE_FAULT = 0xE,
    IRQ_VIRT_RESERVED = 0xF,
    IRQ_VIRT_X87_FPU_ERROR = 0x10,
    IRQ_VIRT_ALIGNMENT_CHECK = 0x11,
    IRQ_VIRT_MACHINE_CHECK = 0x12,
    IRQ_VIRT_SIMD_IRQ_VIRT = 0x13,
    IRQ_VIRT_VIRTUALIZATION_IRQ_VIRT = 0x14,
    IRQ_VIRT_CONTROL_PROTECTION_IRQ_VIRT = 0x15,
    IRQ_VIRT_FREE1_START = 0x16,
    IRQ_VIRT_FREE1_END = 0x1F,
    IRQ_VIRT_EXCEPTION_END = 0x1F,
    IRQ_VIRT_PIT = 0x20,
    IRQ_VIRT_PS2_FIRST_DEVICE = 0x21,
    IRQ_VIRT_CASCADE = 0x22,
    IRQ_VIRT_COM2 = 0x23,
    IRQ_VIRT_COM1 = 0x24,
    IRQ_VIRT_LPT2 = 0x25,
    IRQ_VIRT_FLOPPY = 0x26,
    IRQ_VIRT_LPT1 = 0x27,
    IRQ_VIRT_CMOS = 0x28,
    IRQ_VIRT_FREE2_START = 0x29,
    IRQ_VIRT_FREE2_END = 0x2B,
    IRQ_VIRT_PS2_SECOND_DEVICE = 0x2C,
    IRQ_VIRT_FPU = 0x2D,
    IRQ_VIRT_PRIMARY_ATA_HARD_DRIVE = 0x2E,
    IRQ_VIRT_SECONDARY_ATA_HARD_DRIVE = 0x2F,
    IRQ_VIRT_FREE3_START = 0x30,
    IRQ_VIRT_FREE3_END = 0xF9,
    IRQ_VIRT_TLB_SHOOTDOWN = 0xFA, ///< TLB shootdown IRQ_VIRT.
    IRQ_VIRT_DIE = 0xFB,           ///< Kills and frees the current thread.
    IRQ_VIRT_NOTE = 0xFC,          ///< Notify that a note is available.
    IRQ_VIRT_TIMER = 0xFD,         ///< The timer subsystem IRQ_VIRT.
    IRQ_VIRT_HALT = 0xFE,          ///< Halt the CPU.
    IRQ_VIRT_AMOUNT = 0xFF
} irq_virt_t;

typedef struct
{
    
} irq_chip_t;

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
 * This function is called from `IRQ_VIRT_handler()` when an IRQ is received. It will call all registered callbacks
 * for the IRQ.
 *
 * @param frame The IRQ_VIRT frame of the IRQ.
 */
void irq_dispatch(IRQ_VIRT_frame_t* frame);

/**
 * @brief Install an IRQ handler.
 *
 * Installs the provided callback function to be called when the specified IRQ is received. The `data` pointer will be
 * passed to the callback when it is called.
 *
 * If the IRQ_VIRT has not yet been redirected the ioapic will be set to redirect the vector.
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
