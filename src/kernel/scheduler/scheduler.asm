[bits 64]

global scheduler_yield_to_user_space
global scheduler_idle_loop

extern io_pic_clear_mask

section .text

;rdi = stackTop
scheduler_yield_to_user_space:
	mov rsp, rdi
	
	mov rdi, 0
	call io_pic_clear_mask
scheduler_idle_loop:
	hlt
	jmp scheduler_idle_loop
