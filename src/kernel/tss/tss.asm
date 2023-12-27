[bits 64]

global tss_load

section .text

tss_load:
    mov ax, 0x28
    ltr ax
    ret