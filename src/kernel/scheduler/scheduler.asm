[bits 64]
global scheduler_yield_to_user_space
global scheduler_idle_process

extern io_pic_clear_mask
extern interrupt_stack_get_top

section .text

scheduler_yield_to_user_space:
	call interrupt_stack_get_top
	mov rsp, rax
	
	mov rdi, 0
	call io_pic_clear_mask
scheduler_idle_process:
	hlt
	jmp scheduler_idle_process
