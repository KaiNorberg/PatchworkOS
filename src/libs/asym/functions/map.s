[bits 64]

%include "lib-syscall.inc"

section .text

global map
map:
    mov rax, SYS_MAP
    int 0x80
    ret