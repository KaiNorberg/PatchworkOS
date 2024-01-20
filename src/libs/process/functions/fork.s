[bits 64]

section .text

global fork
fork:
    mov rax, 6
    int 0x80
    ret