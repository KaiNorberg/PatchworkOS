[bits 64]

section .text

extern syscall_handler_c

global syscall_handler
syscall_handler:
    ;TODO: All of this
    call syscall_handler_c
    iretq