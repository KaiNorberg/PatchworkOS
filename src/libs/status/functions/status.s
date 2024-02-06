[bits 64]

section .text

global status
status:
    mov rax, 3
    int 0x80
    ret