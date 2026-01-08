.code64
.intel_syntax noprefix

#include "kernel/cpu/gdt.h"

.global gdt_load_descriptor
.type gdt_load_descriptor, @function

.text

// rdi = gdt descriptor
gdt_load_descriptor:
    lgdt  [rdi]
    push GDT_KERNEL_CODE
    lea rax, [rip + .reload_CS]
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