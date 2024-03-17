[bits 64]

%include "lib-system.inc"

section .text

global sys_status
sys_status:
    mov rax, SYS_STATUS
    int SYSCALL_VECTOR
    ret