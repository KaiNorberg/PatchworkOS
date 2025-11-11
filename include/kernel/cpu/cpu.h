#pragma once

#include <kernel/config.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/tss.h>
#include <kernel/defs.h>
#include <kernel/drivers/perf.h>
#include <kernel/drivers/rand.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>

#include <stdint.h>
#include <sys/list.h>

/**
 * @brief CPU
 * @defgroup kernel_cpu CPU
 * @ingroup kernel
 *
 * @{
 */

/**
 * @brief Maximum number of CPUs supported.
 */
#define CPU_MAX (UINT8_MAX + 1)

/**
 * @brief ID of the bootstrap CPU.
 */
#define CPU_ID_BOOTSTRAP 0

/**
 * @brief Invalid CPU ID.
 */
#define CPU_ID_INVALID UINT16_MAX

/**
 * @brief Type used to identify a CPU.
 */
typedef uint16_t cpuid_t;

/**
 * @brief CPU stack canary value.
 *
 * Placed at the bottom of CPU stacks, we then check in the interrupt handler if any of the stacks have overflowed by
 * checking if its canary has been modified.
 */
#define CPU_STACK_CANARY 0x1234567890ABCDEFULL

/**
 * @brief CPU structure.
 * @struct cpu_t
 *
 * We allocate the stack buffers inside the `cpu_t` structure to avoid memory allocation during early boot.
 *
 * Must be stored aligned to a page boundary.
 */
typedef struct cpu
{
    cpuid_t id;
    tss_t tss;
    vmm_cpu_ctx_t vmm;
    interrupt_ctx_t interrupt;
    perf_cpu_ctx_t perf;
    timer_cpu_ctx_t timer;
    wait_cpu_ctx_t wait;
    sched_cpu_ctx_t sched;
    rand_cpu_ctx_t rand;
    ipi_cpu_ctx_t ipi;
    stack_pointer_t exceptionStack;
    stack_pointer_t doubleFaultStack;
    stack_pointer_t interruptStack;
    uint8_t exceptionStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t doubleFaultStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t interruptStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
} cpu_t;

/**
 * @brief Array of pointers to cpu_t structures for each CPU, indexed by CPU ID.
 *
 * We make this global since its accessed very frequently, so its a slight optimization.
 */
extern cpu_t* _cpus[CPU_MAX];

/**
 * @brief The number of CPUs currently identified.
 *
 * Use `cpu_amount()` over this variable.
 */
extern uint16_t _cpuAmount;

/**
 * @brief Allows the kernel to identify the current CPU.
 *
 * This function will generate a new CPU ID and make the pointer to the `cpu_t` structure available through the CPU ID
 * MSR.
 *
 * @warning Its important to be careful when identifying CPUs, as the `_cpus` array is not protected by any locks as a
 * optimization due to how frequently its accessed. Thus, only identify CPUs when its known that no other CPUs are
 * active, such as during early boot.
 *
 * @param cpu The CPU structure to initialize.
 */
void cpu_identify(cpu_t* cpu);

/**
 * @brief Initializes a CPU structure as part of the boot process.
 *
 * Must be called on the CPU that will be represented by the `cpu` structure, after setting the CPU ID MSR using
 * `cpu_identify()`.
 *
 * @warning See `cpu_identify()` for warnings about concurrency.
 *
 * @param cpu The CPU structure to initialize.
 * @param id The ID of the CPU.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t cpu_init(cpu_t* cpu);

/**
 * @brief Checks for CPU stack overflows.
 *
 * Checks the canary values at the bottom of each CPU stack and if its been modified panics.
 *
 * @param cpu The CPU to check.
 */
void cpu_stacks_overflow_check(cpu_t* cpu);

/**
 * @brief Halts the current CPU.
 *
 * The CPU will be indefinitely halted and be unrecoverable.
 */
_NORETURN void cpu_halt(void);

/**
 * @brief Halts all other CPUs.
 */
void cpu_halt_others(void);

/**
 * @brief Gets the number of identified CPUs.
 *
 * Use this over `_cpuAmount`.
 *
 * @return The number of identified CPUs.
 */
static inline uint64_t cpu_amount(void)
{
    return _cpuAmount;
}

/**
 * @brief Gets the current CPU structure.
 *
 * Disables interrupts to prevent migration to another CPU.
 *
 * Should be followed be a call to `cpu_put()` to re-enable interrupts.
 *
 * @return A pointer to the current CPU structure.
 */
static inline cpu_t* cpu_get(void)
{
    interrupt_disable();
    cpuid_t id = (cpuid_t)msr_read(MSR_CPU_ID);
    cpu_t* cpu = _cpus[id];
    assert(cpu != NULL && cpu->id == id);
    return cpu;
}

/**
 * @brief Releases the current CPU structure.
 *
 * Re-enables interrupts.
 */
static inline void cpu_put(void)
{
    interrupt_enable();
}

/**
 * @brief Gets the current CPU structure without disabling interrupts.
 *
 * @warning This function does not disable interrupts, it should thus only be used when interrupts are already disabled.
 *
 * @return A pointer to the current CPU structure.
 */
static inline cpu_t* cpu_get_unsafe(void)
{
    cpuid_t id = (cpuid_t)msr_read(MSR_CPU_ID);
    cpu_t* cpu = _cpus[id];
    assert(cpu != NULL && cpu->id == id);
    return cpu;
}

/**
 * @brief Gets the next CPU in the CPU array.
 *
 * Wraps around to the first CPU if the current CPU is the last one.
 *
 * @param current The current CPU.
 * @return A pointer to the next CPU.
 */
static inline cpu_t* cpu_get_next(cpu_t* current)
{
    cpuid_t nextId = current->id + 1;
    if (nextId >= _cpuAmount)
    {
        nextId = 0;
    }
    return _cpus[nextId];
}

/**
 * @brief Macro to iterate over all CPUs.
 *
 * The main reason for using this macro is to avoid changes the the internal implementation of how CPUs are stored
 * affecting other parts of the code.
 *
 * @param cpu Loop variable, a pointer to the current `cpu_t`.
 */
#define CPU_FOR_EACH(cpu) \
    for (cpuid_t _cpuId = 0; _cpuId < _cpuAmount; _cpuId++) \
        for (cpu_t* cpu = _cpus[_cpuId]; cpu != NULL; cpu = NULL)

/** @} */
