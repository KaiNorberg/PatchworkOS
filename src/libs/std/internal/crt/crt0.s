[bits 64]

extern main
extern exit
extern _init

section .text
global _start:function (_start.end - _start)
_start:
    call _init

    call main
    
    xor rdi, rdi
    call exit
    ud2
.end: