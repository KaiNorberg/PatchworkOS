[bits 64]

%include "lib-system.inc"

extern scheduler_yield

extern syscall_handler_end
extern syscallTable

section .text

global syscall_handler
syscall_handler:
    cld
    cmp rax, SYS_TOTAL_AMOUNT
    jge .not_available

    call [syscallTable + rax * 8]
    call syscall_handler_end

    iretq
.not_available:
    mov rax, -1
    iretq