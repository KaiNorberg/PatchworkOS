[bits 64]

section .text

global sched_idle_loop
sched_idle_loop:
    sti
    hlt
    jmp sched_idle_loop
