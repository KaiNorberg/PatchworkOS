[bits 64]

extern kernelAddressSpace

extern syscall_handler
global syscall_interrupt
syscall_interrupt:
    push r11
	push r10
	push r9
	push r8
	push rdi
	push rsi
	push rcx
	push rdx

    call syscall_handler
	
	push rax
    mov al,20h
    out 20h,al
	pop rax
	pop rdx
	pop rcx
	pop rsi
	pop rdi
	pop r8
	pop r9
	pop r10
	pop r11
	iretq