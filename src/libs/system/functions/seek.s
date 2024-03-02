[bits 64]

%include "lib-syscall.inc"

section .text

global seek
seek:
    mov rax, SYS_SEEK
    int 0x80
    ret