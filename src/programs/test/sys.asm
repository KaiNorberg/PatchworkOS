[bits 64]

global sys_test
global sys_fork

sys_test:
    mov rax, 1000
    int 0x80
    ret

sys_fork:
    mov rax, 57
    int 0x80
    ret

sys_wait:
    mov rax, 35
    int 0x80
    ret