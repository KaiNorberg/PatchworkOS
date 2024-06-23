[bits 64]

%include "gdt.inc"

section .text

;rdi = gdt descriptor
global gdt_load_descriptor
gdt_load_descriptor:
    lgdt  [rdi]
    push GDT_KERNEL_CODE
    lea rax, [rel .reload_CS]
    push rax
    retfq
.reload_CS:
    mov ax, GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret
