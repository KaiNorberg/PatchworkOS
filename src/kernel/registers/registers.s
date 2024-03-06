[bits 64]

section .text

global rflags_read
rflags_read:
    pushfq
    pop rax
    ret

;rdi = rflags
global rflags_write
rflags_write:
    push rdi
    popfq
    ret