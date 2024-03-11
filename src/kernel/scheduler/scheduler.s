[bits 64]

%include "utils/utils.inc"

extern scheduler_schedule
extern debug_panic

section .text

global scheduler_idle_loop
scheduler_idle_loop:
	hlt
	jmp scheduler_idle_loop

global scheduler_yield
scheduler_yield:
    int 0x92
    ret
    
	mov qword [rsp - 8], 0x10
	mov qword [rsp - 16], rsp
	sub rsp, 16
    pushfq
    push 0x08
    push .return

    sub rsp, 16
    PUSH_ALL_REGS

    mov rdi, rsp
    call scheduler_schedule

    POP_ALL_REGS
    add rsp, 16

	iretq
.return:
	ret