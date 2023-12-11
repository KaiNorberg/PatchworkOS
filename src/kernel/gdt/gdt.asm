[bits 64]
global gdt_load
global tss_load

section .text

gdt_load:
    lgdt [rdi]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    pop rdi
    mov rax, 0x08
    push rax
    push rdi
    retfq

tss_load:
    mov ax, 0x28
    ltr ax
    ret