[bits 64]

global gdt_load_descriptor

section .text

;rdi = gdt descriptor
gdt_load_descriptor:    
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
