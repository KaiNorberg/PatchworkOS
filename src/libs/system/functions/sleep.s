[bits 64]

%include "lib-syscall.inc"

section .text

global sleep
sleep:
    mov rax, SYS_SLEEP
    int 0x80
    ret