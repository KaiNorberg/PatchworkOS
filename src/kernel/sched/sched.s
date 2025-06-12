[bits 64]

%include "cpu/trap.inc"

section .text

extern sched_schedule
extern smp_self_unsafe

global sched_idle_loop
sched_idle_loop:
    sti
    hlt
    jmp sched_idle_loop

global sched_invoke
sched_invoke:
    TRAP_FRAME_CONSTRUCT

    cli

    call smp_self_unsafe
    mov rsi, rax

    mov rdi, rsp
    call sched_schedule
    cmp rax, 0
    jnz .scheduling_occoured

    sti
    add rsp, 176 ; Pop the entire trap frame and discard
    ret
.scheduling_occoured:
    TRAP_FRAME_REGS_POP
    add rsp, 16
    iretq ; Original rflags are loaded here so the lack of a "sti" operation does not matter.
