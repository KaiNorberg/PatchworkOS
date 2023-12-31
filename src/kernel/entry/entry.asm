[bits 64]

global _start

extern main

section .text
_start:
    cld

    mov rsp, stack_top
    mov rbp, 0

    call main
.halt:
    hlt
    jmp .halt

section .bss
stack_bottom:
resb 0x1000
stack_top: