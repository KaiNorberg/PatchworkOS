[bits 64]

%include "lib-syscall.inc"

section .text

extern syscallTable

global syscall_handler
syscall_handler:
    cld
    cmp rax, SYS_TOTAL_AMOUNT
    jge .not_available
    call [syscallTable + rax * 8]
    ret
.not_available:
    mov rax, -1
    ret