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
 * but for the sake of simplicity we reserve a single interrupt vector `IRQ_VIRT_IPI` for IPIs which, when received,
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
     * @brief Should invoke the given virtual IRQ on the target CPU.
     *
     * Should panic on failure.
     *
     * @param cpu The target CPU.
     * @param virt The virtual IRQ to invoke.
     */
    void (*invoke)(cpu_t* cpu, irq_virt_t virt);
} ipi_chip_t;

/**
 * @brief IPI function data structure.
 * @struct ipi_func_data_t
 *
 * Data passed to an IPI function when invoked.
 */
typedef irq_func_data_t ipi_func_data_t;

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
    void* private;
} ipi_t;

/**
 * @brief IPI queue size.
 */
#define IPI_QUEUE_SIZE 16

/**
 * @brief Per-CPU IPI context.
 * @struct ipi_cpu_ctx_t
 *
 * Stores the IPIs received by the owner CPU.
 */
typedef struct ipi_cpu_ctx
{
    ipi_t queue[IPI_QUEUE_SIZE];
    uint64_t readIndex;
    uint64_t writeIndex;
    lock_t lock;
} ipi_cpu_ctx_t;

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
void ipi_cpu_ctx_init(ipi_cpu_ctx_t* ctx);

/**
 * @brief Initialize the IPI subsystem.
 */
void ipi_init(void);

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
 * @brief Add an IPI to the CPU's IPI queue and notify the CPU.
 *
 * The CPU is notified of the IPI by receiving a `IRQ_VIRT_IPI` interrupt.
 *
 * @param cpu The specified CPU, check `ipi_flags_t`.
 * @param flags The flags for how to send the IPI.
 * @param func The function to execute on target CPU(s).
 * @param private The private data to pass to the function, will be found in `irq_func_data_t->private`.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENODEV`: No IPI chip is registered.
 * - `ENOSYS`: The registered IPI chip does not have a `notify` function.
 * - `EBUSY`: The target CPU's IPI queue is full, some or all IPIs could not be sent.
 * - Other errors returned by the IPI chip's `notify` function.
 */
uint64_t ipi_send(cpu_t* cpu, ipi_flags_t flags, ipi_func_t func, void* private);

/**
 * @brief Trigger the specified virtual IRQ on the given CPU.
 *
 * @param cpu The target CPU.
 * @param virt The virtual IRQ to invoke.
 */
void ipi_invoke(cpu_t* cpu, irq_virt_t virt);

/** @} */