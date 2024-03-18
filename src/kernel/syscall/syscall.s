[bits 64]

%include "internal/syscalls/syscalls.inc"

extern syscall_handler_end
extern syscallTable

section .text

global syscall_handler
syscall_handler:
    cld
    cmp rax, SYSCALL_AMOUNT
    jge .not_available

    call [syscallTable + rax * 8]
    call syscall_handler_end

    iretq
.not_available:
    mov rax, -1
    iretq