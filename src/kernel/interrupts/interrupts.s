[bits 64]

%macro INT_NAME 1
    dq interrupt_%1
%endmacro

%macro INT_ERROR_CODE 1
interrupt_%1:
    push qword %1
    jmp common_interrupt
%endmacro

%macro INT_NO_ERROR_CODE 1
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

    mov rdi, rsp
    call interrupt_handler
    
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

INT_NO_ERROR_CODE 0
INT_NO_ERROR_CODE 1
INT_NO_ERROR_CODE 2
INT_NO_ERROR_CODE 3
INT_NO_ERROR_CODE 4
INT_NO_ERROR_CODE 5
INT_NO_ERROR_CODE 6
INT_NO_ERROR_CODE 7
INT_ERROR_CODE 8
INT_NO_ERROR_CODE 9
INT_ERROR_CODE 10
INT_ERROR_CODE 11
INT_ERROR_CODE 12
INT_ERROR_CODE 13
INT_ERROR_CODE 14
INT_NO_ERROR_CODE 15
INT_NO_ERROR_CODE 16
INT_ERROR_CODE 17
INT_NO_ERROR_CODE 18
INT_NO_ERROR_CODE 19
INT_NO_ERROR_CODE 20
INT_NO_ERROR_CODE 21
INT_NO_ERROR_CODE 22
INT_NO_ERROR_CODE 23
INT_NO_ERROR_CODE 24
INT_NO_ERROR_CODE 25
INT_NO_ERROR_CODE 26
INT_NO_ERROR_CODE 27
INT_NO_ERROR_CODE 28
INT_NO_ERROR_CODE 29
INT_ERROR_CODE 30
INT_NO_ERROR_CODE 31

%assign i 32
%rep 224
    INT_NO_ERROR_CODE i
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