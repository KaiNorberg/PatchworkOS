#pragma once

#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/irq.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>

#include <stdbool.h>
#include <stdint.h>
#include <sys/list.h>

typedef struct cpu cpu_t;

/**
 * @brief Inter-Processor Interrupts (IPIs)
 * @defgroup kernel_cpu_ipi IPI
 * @ingroup kernel_cpu
 *
 * Inter-Processor Interrupts are a way to remotely interrupt another CPU, this could be done with any interrupt vector,
 * but for the sake of simplicity we reserve a single interrupt vector `VECTOR_IPI` for IPIs which, when received,
 * will cause the CPU to check its IPI queue for any pending IPIs to execute.
 *
 * The actual remote interrupt invocation of the IPI is handled by a "IPI chip", usually the local APIC, which is
 * implemented in a module.
 *
 * @{
 */

/**
 * @brief Inter-Processor Interrupt (IPI) chip structure.
 * @struct ipi_chip_t
 *
 * Represents a implemented hardware IPI controller, such as the local APIC.
 */
typedef struct ipi_chip
{
    const char* name;
    /**
     * @brief Should interrupt the given CPU with the given virtual IRQ.
     *
     * Should panic on failure.
     *
     * @param cpu The target CPU.
     * @param virt The virtual IRQ to interrupt the CPU with.
     */
    void (*interrupt)(cpu_t* cpu, irq_virt_t virt);
    void (*ack)(void);
    void (*eoi)(void);
} ipi_chip_t;

/**
 * @brief IPI function data structure.
 * @struct ipi_func_data_t
 *
 * Data passed to an IPI function when invoked.
 */
typedef struct
{
    void* data;
} ipi_func_data_t;

/**
 * @brief IPI function type.
 */
typedef void (*ipi_func_t)(ipi_func_data_t* data);

/**
 * @brief IPI structure.
 * @struct ipi_t
 *
 * Represents a single IPI to be executed on a CPU.
 */
typedef struct ipi
{
    ipi_func_t func;
    void* data;
} ipi_t;

/**
 * @brief IPI queue size.
 */
#define IPI_QUEUE_SIZE 16

/**
 * @brief Per-CPU IPI context.
 * @struct ipi_cpu_t
 *
 * Stores the IPIs received by the owner CPU.
 */
typedef struct ipi_cpu_ctx
{
    ipi_t queue[IPI_QUEUE_SIZE];
    size_t readIndex;
    size_t writeIndex;
    lock_t lock;
} ipi_cpu_t;

/**
 * @brief IPI flags.
 * @enum ipi_flags_t
 */
typedef enum
{
    IPI_SINGLE = 0 << 0,    ///< Send the IPI to the specified CPU.
    IPI_BROADCAST = 1 << 0, ///< Send the IPI to all CPUs, specified CPU ignored.
    IPI_OTHERS = 2 << 0,    ///< Send the IPI to all CPUs except the specified CPU.
} ipi_flags_t;

/**
 * @brief Initialize per-CPU IPI context.
 *
 * @param ctx The IPI context to initialize.
 */
void ipi_cpu_init(ipi_cpu_t* ctx);

/**
 * @brief Handle pending IPIs on the current CPU.
 *
 * @param frame The interrupt frame.
 */
void ipi_handle_pending(interrupt_frame_t* frame);

/**
 * @brief Register an IPI chip.
 *
 * There can only be a single IPI chip registered at a time.
 *
 * @param chip The IPI chip to register.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EBUSY`: An IPI chip is already registered.
 */
uint64_t ipi_chip_register(ipi_chip_t* chip);

/**
 * @brief Unregister the IPI chip.
 *
 * If the given chip is not the registered chip, this is a no-op.
 *
 * @param chip The IPI chip to unregister, or `NULL` for no-op.
 */
void ipi_chip_unregister(ipi_chip_t* chip);

/**
 * @brief Get the number of registered IPI chips.
 *
 * Will always be `0` or `1`.
 *
 * @return The number of registered IPI chips.
 */
uint64_t ipi_chip_amount(void);

/**
 * @brief Send an IPI to one or more CPUs.
 *
 * The CPU(s) is notified of the IPI by receiving a `VECTOR_IPI` interrupt.
 *
 * @param cpu The specified CPU, check `ipi_flags_t`.
 * @param flags The flags for how to send the IPI.
 * @param func The function to execute on target CPU(s).
 * @param private The private data to pass to the function, will be found in `irq_func_data_t->data`.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENODEV`: No IPI chip is registered.
 * - `ENOSYS`: The registered IPI chip does not have a `notify` function.
 * - `EBUSY`: The target CPU's IPI queue is full, some or all IPIs could not be sent.
 * - Other errors returned by the IPI chip's `notify` function.
 */
uint64_t ipi_send(cpu_t* cpu, ipi_flags_t flags, ipi_func_t func, void* data);

/**
 * @brief Wake up one or more CPUs.
 *
 * A wake-up IPI is an IPI with no function to execute, used to wake up a CPU that may be idle or sleeping and to prompt
 * it to check for pending IPIs, notes, etc.
 *
 * @param cpu The specified CPU, check `ipi_flags_t`.
 * @param flags The flags for how to send the IPI.
 */
void ipi_wake_up(cpu_t* cpu, ipi_flags_t flags);

/**
 * @brief Invoke a IPI interrupt on the current CPU.
 *
 * Will use `IRQ_INVOKE(VECTOR_IPI)` to trigger the IPI interrupt, causing the CPU to enter an interrupt context and
 * handle any pending IPIs, notes and potentially scheduling.
 */
void ipi_invoke(void);

/** @} */