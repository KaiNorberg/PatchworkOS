#pragma once

#include <assert.h>
#include <kernel/cpu/cpu.h>
#include <kernel/cpu/percpu.h>
#include <kernel/cpu/regs.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Clear Interrupt Flag (CLI) Handling
 * @defgroup kernel_cpu_cli CLI
 * @ingroup kernel_cpu
 *
 * Manages nested CLI (Clear Interrupt Flag) calls.
 * @{
 */

/**
 * @brief Increments the CLI depth, disabling interrupts if depth was zero.
 *
 * @warning Must have a matching `cli_pop()` call to re-enable interrupts when depth reaches zero.
 */
static inline void cli_push(void)
{
    uint64_t rflags = rflags_read();
    ASM("cli" :: : "memory");

    if (SELF->cli == 0)
    {
        SELF->oldRflags = rflags;
    }
    SELF->cli++;
}

/**
 * @brief Decrements the CLI depth, re-enabling interrupts if depth reaches zero and interrupts were enabled prior to
 * the first `cli_push()` call.
 *
 * @warning This function should only be called after a `cli_push()` call.
 */
static inline void cli_pop(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));
    assert(SELF->cli != 0);
    SELF->cli--;
    if (SELF->cli == 0 && (SELF->oldRflags & RFLAGS_INTERRUPT_ENABLE))
    {
        ASM("sti" :: : "memory");
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