extern main
extern exit
extern _init
extern _StdInit

section .text
global _start:function (_start.end - _start)
_start:
    call _init

    call _StdInit

    call main

    mov rdi, rax
    call exit
    ud2
.end: