[bits 64]

%include "cpu/interrupt.inc"

section .text

extern sched_invoke
extern cpu_get_unsafe

global sched_idle_loop
sched_idle_loop:
    sti
    hlt
    jmp sched_idle_loop
