#pragma once

#include <kernel/cpu/regs.h>
#include <kernel/cpu/cpu_id.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

/**
 * @brief Clear Interrupt Flag (CLI) Handling
 * @defgroup kernel_cpu_cli CLI
 * @ingroup kernel_cpu
 * 
 * Manages nested CLI (Clear Interrupt Flag) calls.
 * @{
 */


/**
 * @brief Structure to manage nested CLI calls.
 * @struct cli_t
 */
typedef struct
{
    uint64_t oldRflags;
    uint8_t cli;
} cli_t;

/**
 * @brief Array of per-CPU CLI contexts, indexed by the CPU ID.
 */
extern cli_t _cli[CPU_MAX];

/**
 * @brief Increments the CLI depth, disabling interrupts if depth was zero.
 *
 * @warning Must have a matching `cli_pop()` call to re-enable interrupts when depth reaches zero.
 */
static inline void cli_push(void)
{
    uint64_t rflags = rflags_read();
    asm volatile("cli" ::: "memory");
    cli_t* ctx = &_cli[cpu_get_id()];
    if (ctx->cli == 0)
    {
        ctx->oldRflags = rflags;
    }
    ctx->cli++;
}

/**
 * @brief Decrements the CLI depth, re-enabling interrupts if depth reaches zero and interrupts were enabled prior to the first `cli_push()` call.
 *
 * @warning This function should only be called after a `cli_push()` call.
 */
static inline  void cli_pop(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));
    cli_t* ctx = &_cli[cpu_get_id()];
    assert(ctx->cli != 0);
    ctx->cli--;
    if (ctx->cli == 0 && (ctx->oldRflags & RFLAGS_INTERRUPT_ENABLE))
    {
        asm volatile("sti" ::: "memory");
    }
}

/**
 * @brief Macro to increment CLI depth for the duration of the current scope.
 */
#define CLI_SCOPE() \
    cli_push(); \
    __attribute__((cleanup(cli_scope_cleanup))) int CONCAT(i, __COUNTER__) = 1;

static inline void cli_scope_cleanup(int* _)
{
    cli_pop();
}

/** @} */