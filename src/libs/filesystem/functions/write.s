[bits 64]

%include "lib-syscall.inc"

section .text

global write
write:
    mov rax, SYS_WRITE
    int 0x80
    ret