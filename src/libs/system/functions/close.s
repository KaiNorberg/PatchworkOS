[bits 64]

%include "lib-syscall.inc"

section .text

global close
close:
    mov rax, SYS_CLOSE
    int 0x80
    ret