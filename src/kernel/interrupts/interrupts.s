[bits 64]

%macro INT_NAME 1
    dq interrupt_%1
%endmacro

%macro INT_ERROR 1
interrupt_%1:
    push qword %1
    jmp common_interrupt
%endmacro

%macro INT_NORMAL 1
interrupt_%1:
    push qword 0
    push qword %1
    jmp common_interrupt
%endmacro

section .text

extern interrupt_handler

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

    mov rdi, rsp
    call interrupt_handler

    pop rax
    mov rbx, cr3
    cmp rbx, rax
    je .dont_flush_tlb
    mov cr3, rax
.dont_flush_tlb:

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

INT_NORMAL 0
INT_NORMAL 1
INT_NORMAL 2
INT_NORMAL 3
INT_NORMAL 4
INT_NORMAL 5
INT_NORMAL 6
INT_NORMAL 7
INT_ERROR 8
INT_NORMAL 9
INT_ERROR 10
INT_ERROR 11
INT_ERROR 12
INT_ERROR 13
INT_ERROR 14
INT_NORMAL 15
INT_NORMAL 16
INT_ERROR 17
INT_NORMAL 18
INT_NORMAL 19
INT_NORMAL 20
INT_NORMAL 21
INT_NORMAL 22
INT_NORMAL 23
INT_NORMAL 24
INT_NORMAL 25
INT_NORMAL 26
INT_NORMAL 27
INT_NORMAL 28
INT_NORMAL 29
INT_ERROR 30
INT_NORMAL 31

%assign i 32
%rep 224
    INT_NORMAL i
%assign i i+1
%endrep

section .data

global vectorTable
vectorTable:
%assign i 0
%rep 256
    INT_NAME i
%assign i i+1
%endrep