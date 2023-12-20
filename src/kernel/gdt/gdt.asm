[bits 64]
global gdt_load
global tss_load

section .text

gdt_load:    
    lgdt  [rdi]
    push 0x08
    lea rax, [rel .reload_CS]
    push rax
    retfq
.reload_CS:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

tss_load:
    mov ax, 0x28
    ltr ax
    ret