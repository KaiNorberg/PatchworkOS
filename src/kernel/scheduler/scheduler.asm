[bits 64]

global scheduler_idle_loop

section .text

scheduler_idle_loop:
	hlt
	jmp scheduler_idle_loop
