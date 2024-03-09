[bits 64]

extern scheduler_schedule

section .text

global scheduler_idle_loop
scheduler_idle_loop:
	hlt
	jmp scheduler_idle_loop

%if 0
global scheduler_yield
scheduler_yield:
	;Has to push the stack pointer without modifiying it
	mov qword [rsp - 8], 0x10
	mov qword [rsp - 16], rsp
	sub rsp, 16
    pushfq
    push 0x08
    push .return

    sub rsp, 16

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call scheduler_schedule

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
	iretq
.return:
	ret
%endif