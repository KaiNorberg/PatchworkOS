[bits 64]

global scheduler_yield_to_user_space
global scheduler_idle_loop

extern apic_timer_init

section .text

;rdi = stackTop
scheduler_yield_to_user_space:
	mov rsp, rdi

	call apic_timer_init
scheduler_idle_loop:
	hlt
	jmp scheduler_idle_loop
