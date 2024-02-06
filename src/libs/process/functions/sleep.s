[bits 64]

section .text

global sleep
sleep:
    mov rax, 2
    int 0x80
    ret