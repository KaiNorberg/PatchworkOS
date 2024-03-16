[bits 64]

%include "lib-system.inc"

section .text

global spawn
spawn:
    mov rax, SYS_SPAWN
    int 0x80
    ret