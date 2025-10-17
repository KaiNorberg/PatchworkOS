[bits 64]

%include "cpu/interrupt.inc"

extern syscall_handler

section .text

[bits 64]

%include "cpu/interrupt.inc"

extern syscall_handler

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
global syscall_entry
syscall_entry:
    swapgs
    mov [gs:0x8], rsp
    mov rsp, [gs:0x0]

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

    mov rsp, [gs:0x8]
    swapgs
    o64 sysret
