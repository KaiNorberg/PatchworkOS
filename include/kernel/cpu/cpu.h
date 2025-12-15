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

#include <kernel/utils/map.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/bitmap.h>
#include <sys/list.h>

/**
 * @brief CPU
 * @defgroup kernel_cpu CPU
 * @ingroup kernel
 *
 * CPU structures and functions.
 *
 * ## Events
 *
 * Each CPU can generate events which can be handled by registering event handlers
 * using `cpu_handler_register()`.
 *
 * As an example, the `CPU_ONLINE` event allows other subsystems to perform per-CPU initialization on each CPU no matter
 * when they are initialized or even if IPIs are available yet.
 *
 * This is what makes it possible for SMP to be in a module, at the cost of the system being perhaps slightly
 * unintuitive and edge case heavy.
 *
 * For more details see `cpu_handler_register()`, `cpu_handler_unregister()`, and `cpu_handlers_check()`.
 *
 * ## Per-CPU Data
 *
 * @todo Implement per-CPU data. Use separate linker section?
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
 * @brief CPU event types.
 * @enum cpu_event_type_t
 */
typedef enum
{
    CPU_ONLINE,
    CPU_OFFLINE,
} cpu_event_type_t;

/**
 * @brief CPU event structure.
 */
typedef struct
{
    cpu_event_type_t type;
} cpu_event_t;

/**
 * @brief Maximum number of CPU event handlers that can be registered.
 *
 * We need to statically allocate the event handler array such that handlers can be registered before memory allocation
 * has been initialized.
 */
#define CPU_MAX_EVENT_HANDLERS 32

/**
 * @brief CPU event function type.
 */
typedef void (*cpu_func_t)(cpu_t* cpu, const cpu_event_t* event);

typedef struct
{
    cpu_func_t func;
    BITMAP_DEFINE(initializedCpus, CPU_MAX);
} cpu_handler_t;

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
    /**
     * If set, then since the last check, handlers have been registered or unregistered.
     */
    atomic_bool needHandlersCheck;
    tss_t tss;
    vmm_cpu_ctx_t vmm;
    interrupt_ctx_t interrupt;
    perf_cpu_ctx_t perf;
    timer_cpu_ctx_t timer;
    wait_t wait;
    sched_t sched;
    rand_cpu_ctx_t rand;
    ipi_cpu_ctx_t ipi;
    stack_pointer_t exceptionStack;
    stack_pointer_t doubleFaultStack;
    stack_pointer_t nmiStack;
    stack_pointer_t interruptStack;
    uint8_t exceptionStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t doubleFaultStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t nmiStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
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
 * @brief Only initialize the parts of the CPU structure needed for early boot.
 *
 * The only reason we need this is to split the initialization of the bootstrap CPU to avoid circular dependencies
 * during early boot and since we cant use memory allocation yet.
 *
 * @param cpu The CPU structure to initialize.
 */
void cpu_init_early(cpu_t* cpu);

/**
 * @brief Initializes the CPU represented by the `cpu_t` structure.
 *
 * Must be called on the CPU that will be represented by the `cpu` structure, after setting the CPU ID MSR using
 * `cpu_init_early()`.
 *
 * @param cpu The CPU structure to initialize.
 */
void cpu_init(cpu_t* cpu);

/**
 * @brief Registers a CPU event handler for all CPUs.
 *
 * The registered handler will be immediately invoked with a `CPU_ONLINE` event on the current CPU, then invoked on all
 * others when they call `cpu_handlers_check()` and on any new cpus when they are initialized.
 *
 * @param func The event function to register a handler for.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EBUSY`: Too many event handlers registered.
 * - `EEXIST`: A handler with the same function is already registered.
 */
uint64_t cpu_handler_register(cpu_func_t func);

/**
 * @brief Unregisters a previously registered CPU event handler.
 *
 * Will be a no-op if the handler was not registered.
 *
 * @param func The event function of the handler to unregister, or `NULL` for no-op.
 */
void cpu_handler_unregister(cpu_func_t func);

/**
 * @brief Checks if any handlers have been registered since the last check, and invokes them if so.
 *
 * @param cpu The CPU to check, must be the current CPU.
 */
void cpu_handlers_check(cpu_t* cpu);

/**
 * @brief Checks for CPU stack overflows.
 *
 * Checks the canary values at the bottom of each CPU stack and if its been modified panics.
 *
 * @param cpu The CPU to check.
 */
void cpu_stacks_overflow_check(cpu_t* cpu);

/**
 * @brief Halts all other CPUs.
 *
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t cpu_halt_others(void);

/**
 * @brief Gets the top of the interrupt stack for a CPU.
 *
 * Usefull as we might need to retrieve the interrupt stack in assembly, so this avoid code duplication.
 *
 * @param cpu The CPU to get the interrupt stack top for.
 * @return The top of the interrupt stack.
 */
uintptr_t cpu_interrupt_stack_top(cpu_t* cpu);

/**
 * @brief Gets the number of identified CPUs.
 *
 * Use this over `_cpuAmount`.
 *
 * @return The number of identified CPUs.
 */
static inline uint16_t cpu_amount(void)
{
    return _cpuAmount;
}

/**
 * @brief Gets a CPU structure by its ID.
 *
 * @param id The ID of the CPU to get.
 * @return A pointer to the CPU structure, or `NULL` if no CPU with the given ID exists.
 */
static inline cpu_t* cpu_get_by_id(cpuid_t id)
{
    if (id >= _cpuAmount)
    {
        return NULL;
    }
    return _cpus[id];
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
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    cpuid_t id = (cpuid_t)msr_read(MSR_CPU_ID);
    cpu_t* cpu = _cpus[id];
    assert(cpu != NULL && cpu->id == id);
    return cpu;
}

/**
 * @brief Gets the current CPU ID.
 *
 * @warning This function does not disable interrupts, it should thus only be used when interrupts are already disabled.
 *
 * @return The current CPU ID.
 */
static inline cpuid_t cpu_get_id_unsafe(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    return (cpuid_t)msr_read(MSR_CPU_ID);
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
