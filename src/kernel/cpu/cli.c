#include <assert.h>
#include <kernel/cpu/cli.h>
#include <kernel/cpu/cpu.h>
#include <kernel/cpu/cpu_id.h>
#include <kernel/cpu/percpu.h>
#include <kernel/cpu/regs.h>
#include <stdbool.h>
#include <stdint.h>

void cli_push(void)
{
    uint64_t rflags = rflags_read();
    asm volatile("cli" ::: "memory");

    if (SELF->cli == 0)
    {
        SELF->oldRflags = rflags;
    }
    SELF->cli++;
}

void cli_pop(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));
    assert(SELF->cli != 0);
    SELF->cli--;
    if (SELF->cli == 0 && (SELF->oldRflags & RFLAGS_INTERRUPT_ENABLE))
    {
        asm volatile("sti" ::: "memory");
    }
}