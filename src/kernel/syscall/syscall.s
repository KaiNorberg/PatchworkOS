[bits 64]

%define SYSCALL_AMOUNT 12

extern syscall_handler_end
extern syscallTable

section .text

global syscall_handler
syscall_handler:
    cld
    cmp rax, SYSCALL_AMOUNT
    jge .not_available

    call [syscallTable + rax * 8]
    push rax
    call syscall_handler_end
    pop rax

    iretq
.not_available:
    mov rax, -1
    iretq