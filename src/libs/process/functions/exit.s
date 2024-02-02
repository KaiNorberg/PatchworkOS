[bits 64]

section .text

global exit
exit:
    mov rax, 0
    int 0x80
    ret