[bits 64]

%include "lib-system.inc"

extern main
extern _init

section .text
global _start:function (_start.end - _start)
_start:
    call _init

    call main
    
    mov rdi, rax
    mov rax, SYS_EXIT
    int SYSCALL_VECTOR
    ud2
.end: