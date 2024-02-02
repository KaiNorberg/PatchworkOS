[bits 64]

section .text

global sleep
sleep:
    mov rax, 6
    int 0x80
    ret