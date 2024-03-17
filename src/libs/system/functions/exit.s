[bits 64]

%include "lib-system.inc"

section .text

global sys_exit
sys_exit:
    mov rax, SYS_EXIT
    int SYSCALL_VECTOR
    mov r9, 0x123456789 ;Magic number to identify that exit returned.
    ud2