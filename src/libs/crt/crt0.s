[bits 64]

extern main
extern _init

section .text
global _start:function (_start.end - _start)
_start:
    call _init

    call main
    
    mov rdi, rax
    mov rax, 0
    int 0x80
    ud2
.end: