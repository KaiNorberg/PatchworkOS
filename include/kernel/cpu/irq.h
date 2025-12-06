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
 * The IRQ system is responsible for managing external interrupts in the system (as in vectors [`VECTOR_EXTERNAL_START`,
 * `VECTOR_EXTERNAL_END`)), where the hardware trigger a physical IRQ (`irq_phys_t`) which is then mapped to a virtual
 * IRQ (`irq_virt_t`) using a `irq_chip_t`.
 *
 * ## Physical vs Virtual IRQs
 *
 * The IRQ chips are usually implemented in a driver and they are responsible for the actual physical to virtual
 * mapping.
 *
 * Note that physical to virtual mapping might not be 1:1 and that there could be multiple `irq_chip_t`s in the system.
 *
 * So, for example, say we receive a physical IRQ 1, which is usually the ps2 keyboard interrupt. Lets also say we have
 * a single IRQ chip, the IOAPIC, which is configured to map physical IRQ 1 to virtual IRQ 0x21 on CPU 0. We would then
 * see all handlers registered for virtual IRQ 0x21 being called on CPU 0.
 *
 * @todo Currently, this system is still simplistic. For example, it cant handle trees of IRQ chips, or multiple chips
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
 * @brief Constant representing no physical IRQ.
 */
#define IRQ_PHYS_NONE UINT32_MAX

/**
 * @brief Virtual IRQ numbers.
 * @typedef irq_virt_t
 */
typedef uint8_t irq_virt_t;

/**
 * @brief Data passed to IRQ functions.
 * @struct irq_func_data_t
 */
typedef struct
{
    interrupt_frame_t* frame;
    cpu_t* self;
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
 * @brief IRQ flags.
 * @enum irq_flags_t
 *
 * Specifies the expected behaviour of an IRQ to a IRQ chip.
 */
typedef enum
{
    IRQ_POLARITY_HIGH = 0 << 0, ///< If set, the IRQ is active high.
    IRQ_POLARITY_LOW = 1 << 0,  ///< If set, the IRQ is active low. Otherwise, active high.
    IRQ_TRIGGER_LEVEL = 0 << 1, ///< If set, the IRQ is level triggered.
    IRQ_TRIGGER_EDGE = 1 << 1,  ///< If set, the IRQ is edge triggered. Otherwise, level triggered.
    IRQ_EXCLUSIVE = 0 << 2,     ///< If set, the IRQ is exclusive (not shared).
    IRQ_SHARED = 1 << 2,        ///< If set, the IRQ is shared.
} irq_flags_t;

/**
 * @brief IRQ structure.
 * @struct irq_t
 *
 * Represents a single virtual IRQ mapped to a physical IRQ.
 */
typedef struct irq
{
    irq_phys_t phys;
    irq_virt_t virt;
    irq_flags_t flags;
    cpu_t* cpu; ///< The CPU with affinity for this IRQ, may be `NULL`.
    irq_domain_t* domain;
    uint64_t refCount;
    list_t handlers;
    rwlock_t lock;
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
 * @brief Allocate a virtual IRQ mapped to the given physical IRQ.
 *
 * Will return an existing virtual IRQ if the physical IRQ is already allocated with the same flags and is shared. In
 * this case its reference count will be incremented.
 *
 * Will succeed even if no IRQ chip is registered for the given physical IRQ, in such a case, the IRQ will be enabled
 * only when a appropriate IRQ chip is registered.
 *
 * @note The IRQ will only be enabled if there are registered handlers for it, otherwise it will remain disabled until a
 * handler is registered.
 *
 * @todo CPU load balancing?
 *
 * @param out Pointer to store the allocated virtual IRQ.
 * @param phys The physical IRQ number.
 * @param flags The IRQ flags.
 * @param cpu The target CPU for the IRQ, or `NULL` for the current CPU.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EBUSY`: The IRQ is already allocated with incompatible flags, or is exclusive.
 * - `ENOSPC`: No more virtual IRQs can be allocated.
 * - Other errors as returned by the IRQ chip's `enable` function.
 */
uint64_t irq_virt_alloc(irq_virt_t* out, irq_phys_t phys, irq_flags_t flags, cpu_t* cpu);

/**
 * @brief Free a previously allocated virtual IRQ.
 *
 * The IRQ will be disabled and its handlers freed only when no more references to it exists.
 *
 * @param virt The virtual IRQ to free.
 */
void irq_virt_free(irq_virt_t virt);

/**
 * @brief Change the CPU responsible for an IRQ.
 *
 * @param virt The virtual IRQ to set the affinity for.
 * @param cpu The target CPU for the IRQ.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOENT`: The given virtual IRQ is not a external vector.
 * - `ENODEV`: The IRQ has no associated IRQ chip.
 * - Other  errors as returned by the IRQ chip's `enable` functions.
 */
uint64_t irq_virt_set_affinity(irq_virt_t virt, cpu_t* cpu);

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
 * - Other errors as returned by the IRQ chip's `enable` function.
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
 * If this is the first handler for the IRQ, the IRQ will be enabled.
 *
 * @param virt The virtual IRQ to register the handler for.
 * @param func The handler function to register.
 * @param private The private data to pass to the handler function.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOENT`: The given virtual IRQ is not a external vector.
 * - `EEXIST`: The given handler is already registered for the given virtual IRQ.
 * - `ENOMEM`: Memory allocation failed.
 * - Other errors as returned by the IRQ chip's `enable` function.
 */
uint64_t irq_handler_register(irq_virt_t virt, irq_func_t func, void* private);

/**
 * @brief Unregister an IRQ handler.
 *
 * If there are no more handlers registered for the IRQ, it will be disabled.
 *
 * @param func The handler function to unregister, or `NULL` for no-op.
 * @param virt The virtual IRQ to unregister the handler from.
 */
void irq_handler_unregister(irq_func_t func, irq_virt_t virt);

/**
 * @brief Invoke the given virtual IRQ.
 *
 * @warning Even tho its technically possible to use the `int` instruction with interrupts disabled, doing so will cause
 * a panic in the interrupt handler as a sanity check. Therefore only use this macro with interrupts enabled.
 *
 * @param virt The virtual IRQ to invoke.
 */
#define IRQ_INVOKE(virt) asm volatile("int %0" : : "i"(virt));

/** @} */
