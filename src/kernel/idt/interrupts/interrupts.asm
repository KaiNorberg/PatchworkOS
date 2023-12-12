extern syscall_handler

extern kernelAddressSpace

global syscall_interrupt
syscall_interrupt:

	push qword rbp
	mov rbp, rsp
    push qword r15
    push qword r14
    push qword r13
    push qword r12
	push qword r11
	push qword r10
	push qword r9
	push qword r8
	push qword rdi
	push qword rsi
	push qword rdx
	push qword rcx
	push qword rbx
	push qword rax

    lea rdi, [rbp - 14 * 8] ;Load register buffer (the part of the stack we pushed all the registers to)
	lea	rax, [rbp + 8] ;Load interrupt stack frame
    mov rsi, rax
    call syscall_handler

	pop qword rax
	pop qword rbx
	pop qword rcx
	pop qword rdx
	pop qword rsi
	pop qword rdi
	pop qword r8
	pop qword r9
	pop qword r10
	pop qword r11
    pop qword r12
    pop qword r13
    pop qword r14
    pop qword r15
	pop qword rbp

	iretq