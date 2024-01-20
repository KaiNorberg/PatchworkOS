[bits 64]

section .text

extern main
extern exit

global _start:function (_start.end - _start)
_start:
    call main
    
    mov rdi, rax
    mov rax, 0
    int 0x80
    ud2
.end: