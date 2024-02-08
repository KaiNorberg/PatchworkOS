[bits 64]

%include "lib-syscall.inc"

section .text

global open
open:
    mov rax, SYS_OPEN
    int 0x80
    ret