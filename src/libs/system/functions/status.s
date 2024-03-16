[bits 64]

%include "lib-system.inc"

section .text

global status
status:
    mov rax, SYS_STATUS
    int 0x80
    ret