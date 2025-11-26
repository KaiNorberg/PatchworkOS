[bits 64]

%include "kernel/cpu/interrupt.inc"

%define GS_SYSCALL_RSP    0x0
%define GS_USER_RSP     0x8

extern syscall_handler

global syscall_entry:function

section .text

; rdi = first argument
; rsi = second argument
; rdx = third argument
; r10 = fourth argument
; r8  = fifth argument
; r9  = sixth argument
; rax = syscall number
; rcx = user rip
; r11 = user rflags
; Return value in rax
ALIGN 4096
syscall_entry:
    swapgs
    
    mov [gs:GS_USER_RSP], rsp
    mov rsp, [gs:GS_SYSCALL_RSP]

    push qword [gs:GS_USER_RSP]

    swapgs

    ; We only need to save volatile registers
    push rdi
    push rsi
    push rdx
    push rcx
    push r8
    push r9
    push r10
    push r11

    sti

    mov rcx, r10 ; Fourth argument
    push rax ; Seventh argument (syscall number)
    call syscall_handler
    add rsp, 8 ; Pop seventh argument

    cli

    pop r11
    pop r10
    pop r9
    pop r8
    pop rcx
    pop rdx
    pop rsi
    pop rdi

    pop rsp
    o64 sysret
