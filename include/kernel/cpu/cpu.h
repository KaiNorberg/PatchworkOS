#pragma once

#ifndef __ASSEMBLER__
#include <kernel/config.h>
#include <kernel/cpu/stack_pointer.h>
#include <kernel/cpu/tss.h>

#include <assert.h>
#include <stdint.h>
#include <sys/proc.h>

typedef struct cpu cpu_t;
#endif

/**
 * @brief CPU
 * @defgroup kernel_cpu CPU Management
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
 * @brief The offset of the `id` member in the `cpu_t` structure.
 *
 * Needed to access the CPU ID from assembly code.
 */
#define CPU_OFFSET_ID 0x8

/**
 * @brief Maximum number of CPUs supported.
 */
#define CPU_MAX UINT8_MAX

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
typedef uint16_t cpu_id_t;

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
    stack_pointer_t exceptionStack;
    stack_pointer_t doubleFaultStack;
    stack_pointer_t nmiStack;
    stack_pointer_t interruptStack;
    uint8_t exceptionStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t doubleFaultStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t nmiStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t interruptStackBuffer[CONFIG_INTERRUPT_STACK_PAGES * PAGE_SIZE] ALIGNED(PAGE_SIZE);
    uint8_t percpu[CONFIG_PERCPU_SIZE] ALIGNED(PAGE_SIZE); ///< Buffer used for per-CPU data.
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
 * @brief Initializes a CPU structure.
 *
 * Will note initialize percpu data, use `percpu_update()` after calling this function.
 *
 * @param cpu The CPU structure to initialize.
 */
void cpu_init(cpu_t* cpu);

/**
 * @brief Checks the current CPU for stack overflows.
 *
 * Checks the canary values at the bottom of each CPU stack and if its been modified panics.
 */
void cpu_stacks_overflow_check(void);

/**
 * @brief Halts all other CPUs.
 *
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set.
 */
uint64_t cpu_halt_others(void);

/**
 * @brief Gets the top of the interrupt stack for the current CPU.
 *
 * Usefull as we might need to retrieve the interrupt stack in assembly, so this avoid code duplication.
 *
 * @return The top of the interrupt stack.
 */
uintptr_t cpu_interrupt_stack_top(void);

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
