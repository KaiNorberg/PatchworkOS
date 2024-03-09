[bits 64]

%include "lib-syscall.inc"

section .text

global exit
exit:
    mov rax, SYS_EXIT
    int 0x80
    mov r9, 0x123456789 ;Magic number to identify that exit returned.
    ud2