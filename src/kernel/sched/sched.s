[bits 64]


%include "kernel/cpu/interrupt.inc"

extern sched_do
extern cpu_get_unsafe

global sched_idle_loop:function

section .text

sched_idle_loop:
    sti
    hlt
    jmp sched_idle_loop
