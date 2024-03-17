[bits 64]

%include "lib-system.inc"

;This is temporary and only for testing

section .text

global sys_test
sys_test:
    mov rcx, 10000
.L1:
    pause
    loop .L1
    mov rax, SYS_TEST
    int SYSCALL_VECTOR
    ret