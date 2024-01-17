[bits 64]

section .text

global exit
exit:
    mov rax, 60
    int 0x80
    ud2