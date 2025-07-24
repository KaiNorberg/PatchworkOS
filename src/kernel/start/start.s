[bits 64]

extern kmain

section .text

global _start
_start:
    cli
    cld

    mov rsp, kernel_stack_top
    and rsp, ~0xF
    xor rbp, rbp

    call kmain
.halt:
    hlt
    jmp .halt

section .bss

global kernel_stack_bottom
kernel_stack_bottom:
resb 0x8000
global kernel_stack_top
kernel_stack_top:
