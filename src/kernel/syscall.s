%include "kernel/syscalls.inc"

extern syscall_handler_end
extern syscallTable

section .text

global syscall_handler
syscall_handler:
    cmp rax, SYS_TOTAL_AMOUNT
    jae .not_available

    push rdi
    push rsi
    push rdx
    push rcx
    push r8
    push r9
    push r10
    push r11
    push rbp

    mov rbp, 0

    call [syscallTable + rax * 8]
    push rax
    call syscall_handler_end
    pop rax

    pop rbp
    pop r11
    pop r10
    pop r9
    pop r8
    pop rcx
    pop rdx
    pop rsi
    pop rdi

    iretq
.not_available:
    mov rax, -1
    iretq
