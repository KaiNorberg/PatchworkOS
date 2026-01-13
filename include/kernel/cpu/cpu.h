#pragma once

#ifndef __ASSEMBLER__
#include <kernel/config.h>
#include <kernel/cpu/cpu_id.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/tss.h>
#include <kernel/drivers/perf.h>
#include <kernel/drivers/rand.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/rcu.h>
#include <kernel/utils/map.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/bitmap.h>
#include <sys/defs.h>
#include <sys/list.h>
#endif

/**
 * @brief CPU
 * @defgroup kernel_cpu CPU
 * @ingroup kernel
 *
 * @{
 */

/**
 * @brief The offset of the `self` member in the `cpu_t` structure.
 */
#define CPU_OFFSET_SELF 0x0

/**
 * @brief The offset of the `syscall_ctx_t` pointer in the `cpu_t` structure.
 */
#define CPU_OFFSET_SYSCALL_RSP 0x10

/**
 * @brief The offset of the `userRsp` member in the `cpu_t` structure.
 */
#define CPU_OFFSET_USER_RSP 0x18

#ifndef __ASSEMBLER__

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
    cpu_t* self;
    cpu_id_t id;
    uint64_t syscallRsp;
    uint64_t userRsp;
    volatile bool inInterrupt;
    uint64_t oldRflags; ///< The rflags value before disabling interrupts.
    uint16_t cli;       ///< The CLI depth counter used in `cli_push()` and `cli_pop()`.
    tss_t tss;
    rand_cpu_t rand;
    ipi_cpu_t ipi;
    stack_pointer_t exceptionStack;
    stack_pointer_t doubleFaultStack;
    stack_pointer_t nmiStack;
    stack_pointer_t interruptStack;
    uint8_t exceptionStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t doubleFaultStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t nmiStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t interruptStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t percpu[CONFIG_PERCPU_SIZE] ALIGNED(PAGE_SIZE);
} cpu_t;

static_assert(offsetof(cpu_t, self) == CPU_OFFSET_SELF,
    "CPU_OFFSET_SELF does not match the offset of the self field in cpu_t");

static_assert(offsetof(cpu_t, id) == CPU_OFFSET_ID, "CPU_OFFSET_ID does not match the offset of the id field in cpu_t");

static_assert(offsetof(cpu_t, syscallRsp) == CPU_OFFSET_SYSCALL_RSP,
    "CPU_OFFSET_SYSCALL_RSP does not match the offset of the syscallRsp field in cpu_t");

static_assert(offsetof(cpu_t, userRsp) == CPU_OFFSET_USER_RSP,
    "CPU_OFFSET_USER_RSP does not match the offset of the userRsp field in cpu_t");

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
static inline cpu_t* cpu_get_by_id(cpu_id_t id)
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
 * @warning This function does not disable interrupts, it should thus only be used when interrupts are already disabled.
 *
 * @return A pointer to the current CPU structure.
 */
static inline cpu_t* cpu_get(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    cpu_t* cpu;
    asm volatile("movq %%gs:%P1, %0" : "=r"(cpu) : "i"(CPU_OFFSET_SELF));
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
    cpu_id_t nextId = current->id + 1;
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
    for (cpu_id_t _cpuId = 0; _cpuId < _cpuAmount; _cpuId++) \
        for (cpu = _cpus[_cpuId]; cpu != NULL; cpu = NULL)

#endif

/** @} */
