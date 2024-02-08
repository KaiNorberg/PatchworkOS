[bits 64]

%include "lib-syscall.inc"

section .text

global read
read:
    mov rax, SYS_READ
    int 0x80
    ret