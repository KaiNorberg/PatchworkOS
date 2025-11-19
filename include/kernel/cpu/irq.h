#pragma once

#include <kernel/cpu/interrupt.h>
#include <kernel/sync/rwlock.h>

#include <stdbool.h>
#include <stdint.h>
#include <sys/list.h>

typedef struct cpu cpu_t;

typedef struct irq_chip irq_chip_t;
typedef struct irq_domain irq_domain_t;
typedef struct irq irq_t;

/**
 * @brief Interrupt Requests (IRQs)
 * @defgroup kernel_cpu_irq IRQ
 * @ingroup kernel_cpu
 *
 * For interrupt handling we use a system of IRQs, where the hardware trigger a physical IRQ (`irq_phys_t`) which is
 * then mapped to a virtual IRQ (`irq_virt_t`) using a `irq_chip_t`.
 *
 * ## Physical vs Virtual IRQs
 *
 * The IRQ chips are implemented in a driver and they are responsible for the actual physical to virtual mapping.
 *
 * Note that physical to virtual mapping might not be 1:1 and that there could be multiple `irq_chip_t`s in the system.
 *
 * So, for example, say we receive a physical IRQ 1, which is usually the ps2 keyboard interrupt. Lets also say we have
 * a single IRQ chip, the IOAPIC, which is configured to map physical IRQ 1 to virtual IRQ 0x21 on CPU 0. We would then
 * see all handlers registered for virtual IRQ 0x21 being called on CPU 0.
 *
 * ## External and Internal IRQs
 *
 * There are some exceptions to the physical to virtual mappings, we call these "Internal IRQs". These are mainly
 * exceptions and IPIs used internally by the kernel. As in, page faults, general protection faults, timer interrupts,
 * etc. These "IRQs" dont have mappings and are instead fixed. See `irq_virt_t` for the full list.
 *
 * Note that while we give `irq_virt_t` numbers for exceptions they are not handled by the IRQ system as they are really
 * not IRQs and, since exceptions usually means something when wrong, we want to avoid using as many parts of the kernel
 * as reasonable while handling them.
 *
 * In the CPU exceptions are hardwired to occur when certain conditions are meet, unlike IPIs or external IRQs which are
 * triggered by specialized hardware (`irq_chip_t` or `ipi_chip_t`). Exceptions are handled directly in
 * `interrupt_handler()`.
 *
 * TODO: Currently, this system is still simplistic. For example, it cant handle trees of IRQ chips, or multiple chips
 * handling the same physical IRQs. This should be fixed in the future as needed.
 *
 * @{
 */

/**
 * @brief Physical IRQ numbers.
 * @typedef irq_phys_t
 */
typedef uint32_t irq_phys_t;

/**
 * @brief Central listing of all virtual IRQ numbers.
 * @enum irq_virt_t
 *
 * Lists all IRQs, even ones not handled by the IRQ system.
 */
typedef enum
{
    IRQ_VIRT_EXCEPTION_START = 0x0,
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
    IRQ_VIRT_STACK_SEGMENT_FAULT = 0xC,
    IRQ_VIRT_GENERAL_PROTECTION_FAULT = 0xD,
    IRQ_VIRT_PAGE_FAULT = 0xE,
    IRQ_VIRT_RESERVED = 0xF,
    IRQ_VIRT_X87_FLOATING_POINT_EXCEPTION = 0x10,
    IRQ_VIRT_ALIGNMENT_CHECK = 0x11,
    IRQ_VIRT_MACHINE_CHECK = 0x12,
    IRQ_VIRT_SIMD_FLOATING_POINT_EXCEPTION = 0x13,
    IRQ_VIRT_VIRTUALIZATION_EXCEPTION = 0x14,
    IRQ_VIRT_CONTROL_PROTECTION_EXCEPTION = 0x15,
    IRQ_VIRT_EXCEPTION_END = 0x20,

    IRQ_VIRT_IPI = 0x20,      ///< See @ref kernel_cpu_ipi for more information.
    IRQ_VIRT_DIE = 0x21,      ///< Used by the scheduler to kill the current thread.
    IRQ_VIRT_SCHEDULE = 0x22, ///< Used by the scheduler to schedule a new thread.
    IRQ_VIRT_TIMER = 0x23,    ///< Per-CPU timer interrupt.

    IRQ_VIRT_EXTERNAL_START = 0x30, ///< Start of external interrupts (mapped by IRQ chips).
    IRQ_VIRT_EXTERNAL_END = 0xFF,

    IRQ_VIRT_TOTAL_AMOUNT = 0x100,
} irq_virt_t;

/**
 * @brief Data passed to IRQ functions.
 * @struct irq_func_data_t
 */
typedef struct
{
    interrupt_frame_t* frame;
    cpu_t* self; ///< Will be `NULL` for exceptions.
    irq_virt_t virt;
    void* private;
} irq_func_data_t;

/**
 * @brief Callback function type for IRQs.
 */
typedef void (*irq_func_t)(irq_func_data_t* data);

/**
 * @brief Structure to hold an IRQ function and its data.
 */
typedef struct
{
    list_entry_t entry;
    irq_func_t func;
    void* private;
    irq_virt_t virt;
} irq_handler_t;

/**
 * @brief IRQ descriptor structure.
 * @struct irq_desc_t
 *
 * Represents a single IRQ, mapped from a physical IRQ to a virtual IRQ where the virtual IRQ is decided by the index in
 * the global descriptor array.
 */
typedef struct
{
    irq_t* irq;
    list_t handlers;
    rwlock_t lock;
} irq_desc_t;

/**
 * @brief IRQ flags.
 * @enum irq_flags_t
 *
 * Specifies the expected behaviour of an IRQ to a IRQ chip.
 */
typedef enum
{
    IRQ_FLAGS_POLARITY_HIGH = 0 << 0, ///< If set, the IRQ is active high.
    IRQ_FLAGS_POLARITY_LOW = 1 << 0,  ///< If set, the IRQ is active low. Otherwise, active high.
    IRQ_FLAGS_TRIGGER_LEVEL = 0 << 1, ///< If set, the IRQ is level triggered.
    IRQ_FLAGS_TRIGGER_EDGE = 1 << 1,  ///< If set, the IRQ is edge triggered. Otherwise, level triggered.
} irq_flags_t;

/**
 * @brief IRQ structure.
 * @struct irq_t
 *
 * Represents a single IRQ, mapped from a physical IRQ to a virtual IRQ on a specific CPU using a specified IRQ chip
 * with given flags.
 */
typedef struct irq
{
    list_entry_t entry;
    irq_phys_t phys;
    irq_virt_t virt;
    irq_flags_t flags;
    cpu_t* cpu;
    irq_domain_t* domain;
} irq_t;

/**
 * @brief IRQ domain structure.
 * @struct irq_domain_t
 *
 * Represents a range of physical IRQs managed by a specific IRQ chip.
 */
typedef struct irq_domain
{
    list_entry_t entry;
    irq_chip_t* chip;
    void* private;
    irq_phys_t start; ///< Inclusive
    irq_phys_t end;   ///< Exclusive
} irq_domain_t;

/**
 * @brief IRQ chip structure.
 * @struct irq_chip_t
 *
 * Represents a implemented hardware IRQ controller, such as the IOAPIC.
 */
typedef struct irq_chip
{
    const char* name;
    uint64_t (*enable)(irq_t* irq); ///< Enable the given IRQ, must be defined.
    void (*disable)(irq_t* irq);    ///< Disable the given IRQ, must be defined.
    void (*ack)(irq_t* irq);        ///< Send a acknowledge for the given IRQ.
    void (*eoi)(irq_t* irq);        ///< Send End-Of-Interrupt for the given IRQ.
} irq_chip_t;

/**
 * @brief Invoke the given virtual IRQ.
 *
 * @warning Even tho its technically possible to use the `int` instruction with interrupts disabled, doing so will cause
 * a panic in the interrupt handler as a sanity check. Therefore only use this macro with interrupts enabled.
 *
 * @param virt The virtual IRQ to invoke.
 */
#define IRQ_INVOKE(virt) asm volatile("int %0" : : "i"(virt));

/**
 * @brief Initialize the IRQ subsystem.
 */
void irq_init(void);

/**
 * @brief Dispatch an IRQ.
 *
 * This function is called from `interrupt_handler()` when an IRQ is received. It will call all registered handlers
 * for the IRQ and handle acknowledging and EOI as needed.
 *
 * Should not be called for exceptions.
 *
 * Will panic on failure.
 *
 * @param frame The interrupt frame of the IRQ.
 * @param self The CPU on which the IRQ was received.
 */
void irq_dispatch(interrupt_frame_t* frame, cpu_t* self);

/**
 * @brief Allocate and install an IRQ.
 *
 * Will allocate a new virtual IRQ, map it to the given physical IRQ using the appropriate IRQ chip and enable the IRQ
 * with an affinity to the given CPU.
 *
 * Will succeed even if no IRQ chip is registered for the given physical IRQ, in such a case, the IRQ will be enabled
 * only when a appropriate IRQ chip is registered.
 *
 * @param phys The physical IRQ number.
 * @param flags The IRQ flags.
 * @param cpu The target CPU for the IRQ, or `NULL` for the current CPU.
 * @return On success, the allocated IRQ. On failure, `ERR` and `errno` is set to:
 * - `ENOSPC`: All virtual IRQs are in use.
 * - `ENOMEM`: Memory allocation failed.
 * - Other  errors as returned by the IRQ chip's `enable` function.
 */
irq_t* irq_alloc(irq_phys_t phys, irq_flags_t flags, cpu_t* cpu);

/**
 * @brief Free an allocated IRQ.
 *
 * Will remove and free all handlers and disable the IRQ.
 *
 * @param irq The IRQ to free.
 */
void irq_free(irq_t* irq);

/**
 * @brief Change the CPU responsible for an IRQ.
 *
 * @param irq The IRQ to set the affinity for.
 * @param cpu The target CPU for the IRQ.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - Other  errors as returned by the IRQ chip's `enable` functions.
 */
uint64_t irq_set_affinity(irq_t* irq, cpu_t* cpu);

/**
 * @brief Register an IRQ chip for a range of physical IRQs.
 *
 * The same chip can be registered multiple times for ranges that do not overlap.
 *
 * @param chip The IRQ chip to register.
 * @param start The start of the physical IRQ range.
 * @param end The end of the physical IRQ range.
 * @param private Private data for the IRQ chip, will be found in `irq_t->domain->private`.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EEXIST`: A chip with a domain overlapping the given range is already registered.
 * - `ENOMEM`: Memory allocation failed.
 */
uint64_t irq_chip_register(irq_chip_t* chip, irq_phys_t start, irq_phys_t end, void* private);

/**
 * @brief Unregister all instances of the given IRQ chip within the specified range.
 *
 * Will NOT free any IRQs or handlers associated with the chip(s), but it will disable them. If another chip is
 * registered in the same range, the IRQs will be remapped to that chip.
 *
 * @param chip The IRQ chip to unregister, or `NULL` for no-op.
 * @param start The start of the physical IRQ range.
 * @param end The end of the physical IRQ range.
 */
void irq_chip_unregister(irq_chip_t* chip, irq_phys_t start, irq_phys_t end);

/**
 * @brief Get the number of registered IRQ chips.
 *
 * @return The number of registered IRQ chips.
 */
uint64_t irq_chip_amount(void);

/**
 * @brief Register an IRQ handler for a virtual IRQ.
 *
 * @param virt The virtual IRQ to register the handler for.
 * @param func The handler function to register.
 * @param private The private data to pass to the handler function.
 * @return On success, a pointer to the IRQ handler. On failure, `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOMEM`: Memory allocation failed.
 * - `ENOENT`: The given virtual IRQ is not allocated.
 */
irq_handler_t* irq_handler_register(irq_virt_t virt, irq_func_t func, void* private);

/**
 * @brief Unregister an IRQ handler.
 *
 * @param handler The IRQ handler to unregister, or `NULL` for no-op.
 */
void irq_handler_unregister(irq_handler_t* handler);

/** @} */
