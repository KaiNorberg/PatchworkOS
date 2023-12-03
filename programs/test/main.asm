global _start
_start:
    mov rax, 1234
    int 0x80
    mov rax, 5678
    int 0x80
.loop:
    hlt
    jmp .loop