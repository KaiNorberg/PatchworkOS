[bits 64]

global sched_idle_loop:function

section .text

sched_idle_loop:
    sti
    hlt
    jmp sched_idle_loop
