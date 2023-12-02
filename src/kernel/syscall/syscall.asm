[bits 64]

extern syscallAddressSpace
extern syscallStack

extern syscall_handler
global syscall_interrupt
syscall_interrupt:
    push qword rax
    push qword rbx
    push qword rcx
    push qword rdx
    push qword rbp
    push qword r8
    push qword r9
    push qword r10
    push qword r11
    push qword r12
    push qword r13
    push qword r14
    push qword r15

    mov r15, rsp
    mov r14, cr3

    mov rsp, [syscallStack] ;Load kernel stack
    mov r13, [syscallAddressSpace] ;Load address space
    mov cr3, r13
    
    push r15
    push r14

    call syscall_handler

    pop r14
    pop r15

    mov rsp, r15
    mov cr3, r14

    pop qword r15
    pop qword r14
    pop qword r13
    pop qword r12
    pop qword r11
    pop qword r10
    pop qword r9
    pop qword r8
    pop qword rbp
    pop qword rdx
    pop qword rcx
    pop qword rbx
    pop qword rax
    
	iretq