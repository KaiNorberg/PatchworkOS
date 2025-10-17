[bits 64]

%include "cpu/interrupt.inc"

section .text

extern sched_schedule
extern smp_self_unsafe

global sched_idle_loop
sched_idle_loop:
    sti
    hlt
    jmp sched_idle_loop
