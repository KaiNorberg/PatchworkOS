[bits 64]

global sys_fork

sys_fork:
    mov rax, 57
    int 0x80
    ret