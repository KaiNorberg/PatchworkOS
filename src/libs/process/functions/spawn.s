[bits 64]

section .text

global spawn
spawn:
    mov rax, 5
    int 0x80
    ret