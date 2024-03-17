[bits 64]

%include "lib-system.inc"

section .text

global sys_allocate
sys_map:
    mov rax, SYS_ALLOCATE
    int SYSCALL_VECTOR
    ret