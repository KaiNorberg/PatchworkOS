[bits 64]

extern scheduler_schedule
extern debug_panic

section .text

global scheduler_idle_loop
scheduler_idle_loop:
	hlt
	jmp scheduler_idle_loop

global scheduler_yield
scheduler_yield:    
    ;Check if interrupts are enabled
    pushfq
    pop rax
    test rax, 0x200
    jnz .interrupts_enabled

    mov rdi, error_string
    call debug_panic

.interrupts_enabled:
    cli
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
    sti
	iretq
.return:
	ret

section .data
error_string:
db "scheduler_yield called with interrupts disabled", 0