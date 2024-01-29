[bits 64]

global _start
global kernel_stack_bottom
global kernel_stack_top

extern main

section .text
_start:
    cld

    mov rsp, kernel_stack_top
    mov rbp, 0

    call main
.halt:
    hlt
    jmp .halt

section .bss
kernel_stack_bottom:
resb 0x4000
kernel_stack_top: