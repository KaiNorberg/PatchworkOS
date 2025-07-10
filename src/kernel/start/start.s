[bits 64]

extern main

section .text

global _start
_start:
    cli
    mov rsp, kernel_stack_top
    xor rbp, rbp

    call main
.halt:
    hlt
    jmp .halt

section .bss

global kernel_stack_bottom
kernel_stack_bottom:
resb 0x8000
global kernel_stack_top
kernel_stack_top:
