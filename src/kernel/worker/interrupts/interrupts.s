[bits 64]

%macro INT_NAME 1
    dq interrupt_%1
%endmacro

%macro INT_ERR 1
interrupt_%1:
    push qword %1
    jmp common_interrupt
%endmacro

%macro INT_NOERR 1
interrupt_%1:
    push qword 0
    push qword %1
    jmp common_interrupt
%endmacro

section .worker_interrupts

extern worker_interrupt_handler

global workerInterruptsStart
global workerInterruptsEnd
global workerPageDirectory

workerInterruptsStart:

common_interrupt:
    cld
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
    mov rax, cr3
    push rax

    mov rax, [workerPageDirectory]
    mov cr3, rax

    mov rdi, rsp
    call worker_interrupt_handler

    pop rax
    mov cr3, rax
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

workerPageDirectory:
    dq 0

INT_NOERR 0
INT_NOERR 1
INT_NOERR 2
INT_NOERR 3
INT_NOERR 4
INT_NOERR 5
INT_NOERR 6
INT_NOERR 7
INT_ERR   8
INT_NOERR 9
INT_ERR   10
INT_ERR   11
INT_ERR   12
INT_ERR   13
INT_ERR   14
INT_NOERR 15
INT_NOERR 16
INT_ERR   17
INT_NOERR 18
INT_NOERR 19
INT_NOERR 20
INT_NOERR 21
INT_NOERR 22
INT_NOERR 23
INT_NOERR 24
INT_NOERR 25
INT_NOERR 26
INT_NOERR 27
INT_NOERR 28
INT_NOERR 29
INT_ERR   30
INT_NOERR 31

%assign i 32
%rep 224
    INT_NOERR i
%assign i i+1
%endrep

workerInterruptsEnd:

section .data

global workerVectorTable
workerVectorTable:
%assign i 0
%rep 256
    INT_NAME i
%assign i i+1
%endrep