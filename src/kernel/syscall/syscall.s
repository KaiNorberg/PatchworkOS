[bits 64]


%include "lib-syscall.inc"

extern scheduler_yield
extern syscallTable

section .text

global syscall_handler
syscall_handler:
    cld
    cmp rax, SYS_TOTAL_AMOUNT
    jge .not_available

    call [syscallTable + rax * 8]

    call scheduler_yield

    iretq
.not_available:
    mov rax, -1
    iretq