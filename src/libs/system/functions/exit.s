[bits 64]

%include "lib-syscall.inc"

section .text

global exit
exit:
    mov rax, SYS_EXIT
    int 0x80
    ud2