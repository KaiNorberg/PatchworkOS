[bits 64]

extern main
extern exit
extern _init
extern _StdInit

section .text
global _start:function (_start.end - _start)
_start:
    call _StdInit

    call _init

    call main

    mov rdi, rax
    call exit
    ud2
.end: