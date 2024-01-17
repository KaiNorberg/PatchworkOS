[bits 64]

;This is temporary and only for testing

section .text

global sys_test
sys_test:
    mov rax, 1000
    int 0x80
    ret