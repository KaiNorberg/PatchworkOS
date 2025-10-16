[bits 64]

%include "cpu/gdt.inc"

global tss_load

section .text

tss_load:
    mov ax, GDT_TSS
    ltr ax
    ret
