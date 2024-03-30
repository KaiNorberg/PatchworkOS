[bits 64]

section .text

global sched_idle_loop
sched_idle_loop:
	hlt
	jmp sched_idle_loop