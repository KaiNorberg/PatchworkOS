%pragma once

; The size of a interrupt frame in bytes
%define INTERRUPT_FRAME_SIZE 176

; Will construct the registers section of a interrupt frame
%macro INTERRUPT_FRAME_REGS_PUSH 0
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
%endmacro

; Will pop the registers section of a interrupt frame
%macro INTERRUPT_FRAME_REGS_POP 0
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
%endmacro

%macro INTERRUPT_FRAME_POP_AND_JUMP 0
    INTERRUPT_FRAME_REGS_POP

    add rsp, 16
    iretq
%endmacro

; Constructs a interrupt frame from current cpu state
%macro INTERRUPT_FRAME_CONSTRUCT 0
    xor rax, rax ; Set to zero to prevent weirdness with the ss and cs registers.
    mov r8, [rsp] ; Get the return address.
    ; Store the stack pointer from before this function was called
    mov r9, rsp
    add r9, 8

    ; Construct interrupt frame
    mov ax, ss
    push rax
    push r9
    pushfq
    mov ax, cs
    push rax
    push r8

    ; Ignore vector and error code.
    push qword 0
    push qword 0

    INTERRUPT_FRAME_REGS_PUSH
%endmacro
